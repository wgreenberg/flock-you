import { encode, decode, Encoder } from "@msgpack/msgpack";
import { lookupManufacturerIDName } from '$lib/companyIDs';
import { RecordingStore } from "./recordingStore.svelte";
import { DetectionType, DeviceType, ScanResults, type BLEManufacturerData, type DeviceEvent, type DeviceSummary, type FlockDetectionEvent } from "./scanResults.svelte";
import { getNow } from "./utils";

export enum ScannerStatus {
    disconnected = 'disconnected',
    connected = 'connected',
}

export interface ScanCategory {
    name: string,
    wifi_ssids: string[];
    ouis: number[][];
    ble_names: string[];
    ble_manufacturer_ids: number[];
}

function totalItems(cat: ScanCategory): number {
    let result = 0;
    result += cat.wifi_ssids.length;
    result += cat.ouis.length;
    result += cat.ble_names.length;
    result += cat.ble_manufacturer_ids.length;
    return result;
}

export class ProgressTracker {
    public total: number | undefined = $state(undefined);
    public soFar : number | undefined= $state(undefined);
    public error: string | undefined = $state(undefined);
}

async function writeVal(chr: BluetoothRemoteGATTCharacteristic, val: any) {
    return chr.writeValue(new Uint8Array(encode(val)).buffer);
}

type FoxhuntDeviceFn = (device?: DeviceSummary) => Promise<void>;
type UpdateScanCategoriesFn = (categories: ScanCategory[], progress: ProgressTracker) => Promise<void>;

export enum LocationTrackingStatus {
    WaitingForConsent = 'waitingForConsent',
    Tracking = 'tracking',
    NotSupported = 'notSupported',
    Failed = 'failed',
}

export class Scanner {
    public LAST_SEEN_THRESHOLD_MS = 10_000;
    public errors: string[] = $state([]);
    public currentLocation: GeolocationPosition | undefined = $state(undefined);
    public locationTrackingStatus = $state(LocationTrackingStatus.WaitingForConsent);
    public results = new ScanResults();
    public status: ScannerStatus = $state(ScannerStatus.disconnected);
    public foxhuntedDevice: DeviceSummary | undefined = $state(undefined);

    private lastSeenTimestamp: Map<string, number> = new Map();

    constructor(
        private recordingStore: RecordingStore,
        private foxhuntDeviceHandler: FoxhuntDeviceFn,
        private updateScanCategoryHandler: UpdateScanCategoriesFn
    ) {
    }

    public summarize(
        timeThreshold: number,
        rssiThreshold: number,
        pinnedMACs: string[],
    ): [DeviceSummary[], DeviceSummary[], DeviceSummary[]] {
        const now = getNow();
        let recentDevices = [];
        for (const [mac, timestamp] of this.lastSeenTimestamp) {
            if (now - timestamp < timeThreshold) {
                recentDevices.push(mac);
            }
        }
        return this.results.getDevices(recentDevices, pinnedMACs, rssiThreshold);
    }

    public async watchLocation(): Promise<void> {
        if (!navigator.geolocation) {
            this.locationTrackingStatus = LocationTrackingStatus.NotSupported;
            return;
        }
        let userConsents: Promise<void> = new Promise((success, failure) => {
            navigator.geolocation.watchPosition((location) => {
                this.currentLocation = location;
                success();
            }, (err) => {
                failure(err);
            });
        });
        try {
            await userConsents;
            this.locationTrackingStatus = LocationTrackingStatus.Tracking;
        } catch (err) {
            this.locationTrackingStatus = LocationTrackingStatus.Failed;
            console.error(`failed to track location data: ${err}`);
        }
    }

    public static async setupDummy() {
        const store = await RecordingStore.open();
        const result = new Scanner(
            store,
            (device) => Promise.resolve(),
            (categories, progress) => {
                progress.total = 1;
                progress.soFar = 1;
                return Promise.resolve();
            }
        );
        setInterval(async () => {
            const location = result.currentLocation;
            const now = getNow();
            const rng = Math.random();
            if (rng < 0.1) {
                // TODO emit detection event
            } else if (rng < 0.6) {
                result.appendDeviceEvent({
                    type: DeviceType.ble,
                    location,
                    timestamp: now,
                    rssi: -Math.random() * 100,
                    mac: [0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01],
                    deviceName: 'whatever',
                    manufacturerData: [],
                });
            } else {
                result.appendDeviceEvent({
                    type: DeviceType.wifi,
                    location,
                    timestamp: now,
                    ssid: 'femboy hooters',
                    rssi: -Math.random() * 100,
                    channel: 10,
                    mac: [0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x02],
                    frameType: 0x20,
                });
            }
        }, 100);
        await result.watchLocation();
        result.status = ScannerStatus.connected;
        return result;
    }

    public static async setupFromBLEDevice(): Promise<Scanner> {
        const serviceUUIDAlias = 0xACAB0001;
        const serviceUUID = 0x5F9B34FB; // not entirely clear why this is different
        const scanCharacteristicUUID = 0x0001;
        const foxhuntCharacteristicUUID = 0x0002;
        const updateScanCategoryCharacteristicUUID = 0x0003;
        const device = await navigator.bluetooth.requestDevice({filters: [{ name: "FlockYou", services: [serviceUUIDAlias] }], optionalServices: [serviceUUID] });
        if (!device.gatt) {
            throw new Error(`device has no GATT`);
        }
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(serviceUUID);
        const scanCharacteristic = await service.getCharacteristic(scanCharacteristicUUID);
        await scanCharacteristic.startNotifications();
        const foxhuntCharacteristic = await service.getCharacteristic(foxhuntCharacteristicUUID);
        await foxhuntCharacteristic.startNotifications();
        const updateScanCategoryCharacteristic = await service.getCharacteristic(updateScanCategoryCharacteristicUUID)
        const store = await RecordingStore.open();
        const result = new Scanner(
            store,
            async (device) => {
                if (!device) {
                    await writeVal(foxhuntCharacteristic, "");
                } else {
                    await writeVal(foxhuntCharacteristic, device.mac);
                }
            },
            async (categories, progress) => {
                progress.total = categories.reduce((sum, cat) => totalItems(cat) + sum, 0);
                progress.soFar = 0;
                try {
                    await writeVal(updateScanCategoryCharacteristic, "drop");
                    for (let category of categories) {
                        for (const ssid of category.wifi_ssids) {
                            await writeVal(updateScanCategoryCharacteristic, [
                                "edit",
                                category.name,
                                "ssid",
                                ssid
                            ]);
                            progress.soFar++;
                        }
                        for (const oui of category.ouis) {
                            const ouiBuf = new Uint8Array(oui);
                            await writeVal(updateScanCategoryCharacteristic, [
                                "edit",
                                category.name,
                                "oui",
                                ouiBuf
                            ]);
                            progress.soFar++;
                        }
                        for (const ble_name of category.ble_names) {
                            await writeVal(updateScanCategoryCharacteristic, [
                                "edit",
                                category.name,
                                "ble_name",
                                ble_name
                            ]);
                            progress.soFar++;
                        }
                        for (const ble_id of category.ble_manufacturer_ids) {
                            await writeVal(updateScanCategoryCharacteristic, [
                                "edit",
                                category.name,
                                "ble_id",
                                ble_id
                            ]);
                            progress.soFar++;
                        }
                    }
                    await writeVal(updateScanCategoryCharacteristic, "commit");
                } catch (err) {
                    progress.error = `err: ${err}`;
                }
            }
        );
        scanCharacteristic.addEventListener('characteristicvaluechanged', async (_: Event) => {
            try {
                result.processEvent(scanCharacteristic.value!);
            } catch (err) {
                result.errors.push(`${err}`);
            }
        });
        foxhuntCharacteristic.addEventListener('characteristicvaluechanged', async (_: Event) => {
            console.log('foxhunt changed', foxhuntCharacteristic.value);
        });
        setInterval(() => {
            if (!server.connected) {
                result.status = ScannerStatus.disconnected;
            }
        })
        result.status = ScannerStatus.connected;
        await result.watchLocation();
        return result;
    }

    public async foxhuntDevice(device: DeviceSummary) {
        this.foxhuntedDevice = device;
        await this.foxhuntDeviceHandler(device);
    }

    public async cancelFoxhunt() {
        this.foxhuntedDevice = undefined;
        await this.foxhuntDeviceHandler(undefined);
    }

    public async updateScanCategories(categories: ScanCategory[], progress: ProgressTracker) {
        await this.updateScanCategoryHandler(categories, progress);
    }

    public getLastSeen(mac: number[]): number | undefined {
        return this.lastSeenTimestamp.get(`${mac}`);
    }

    public async persist() {
        await this.recordingStore.updateResults(this.results);
    }

    appendDeviceEvent(event: DeviceEvent) {
        this.lastSeenTimestamp.set(`${event.mac}`, event.timestamp);
        this.results.appendDeviceEvent(event);
        this.persist();
    }

    appendDetectionEvent(event: FlockDetectionEvent) {
        this.results.appendDetectionEvent(event);
        this.persist();
    }

    processEvent(data: DataView) {
        const array: any[] = decode(data) as any[];
        const location = this.currentLocation;
        const now = getNow();
        const eventType = array[0];
        if (eventType === 'bluetooth_le') {
            const manufacturerIDs = array[4];
            const manufacturerDataStrings = array[5];
            const manufacturerData: BLEManufacturerData[] = [];
            for (let i = 0; i < manufacturerIDs.length; i++) {
                const idName = lookupManufacturerIDName(manufacturerIDs[i]);
                const data = Uint8Array.from(manufacturerDataStrings[i]);
                manufacturerData.push({
                    id: manufacturerIDs[i],
                    idName,
                    data,
                });
            }
            this.appendDeviceEvent({
                type: DeviceType.ble,
                location,
                timestamp: now,
                deviceName: array[1],
                rssi: array[2],
                mac: array[3],
                manufacturerData,
            });
        } else if (eventType === 'wifi') {
            this.appendDeviceEvent({
                type: DeviceType.wifi,
                location,
                timestamp: now,
                rssi: array[1],
                ssid: array[2],
                channel: array[3],
                mac: array[4],
                frameType: array[5],
            });
        } else if (eventType === 'detection') {
            const type = DetectionType[array[1] as keyof typeof DetectionType];
            const category: string = array[2];
            const mac: number[] = array[3];
            this.appendDetectionEvent({
                type,
                category,
                mac,
            });
        } else if (eventType === "data_too_large") {
            throw new Error("data payload too large");
        } else {
            throw new Error(`invalid scan event: "${array}"`)
        }
    }
}
