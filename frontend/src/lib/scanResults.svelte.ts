import { decode } from "@msgpack/msgpack";
import ouiData from "oui-data";
import { lookupManufacturerIDName } from '$lib/companyIDs';
import { RecordingStore } from "./recordingStore.svelte";

export enum WifiFrameType {
    beacon = 'beacon',
    probe = 'probe',
}

export enum DeviceType {
    wifi = 'wifi',
    ble = 'ble',
}

export enum DetectionType {
    mac = 'mac',
    ssid = 'ssid',
    name = 'name',
    ble_id = 'ble_id'
}

export interface BLEManufacturerData {
    id: number,
    idName: string | undefined,
    data: Uint8Array,
}

export interface BluetoothDeviceSignal {
    type: DeviceType.ble,
    location: GeolocationPosition | undefined,
    timestamp: number,
    rssi: number,
    mac: number[],
    deviceName: string,
    manufacturerData: BLEManufacturerData[],
}

export interface WifiDeviceSignal {
    type: DeviceType.wifi,
    timestamp: number,
    location: GeolocationPosition | undefined,
    ssid: string,
    rssi: number,
    channel: number,
    mac: number[],
    frameType: number,
}

export interface NameDetection {
    type: DetectionType.name,
    mac: number[],
    name: string,
    matchedName: string,
}

export interface SSIDDetection {
    type: DetectionType.ssid,
    mac: number[],
    ssid: string,
    matchedSSID: string,
}

export interface MACDetection {
    type: DetectionType.mac,
    mac: number[],
    matchedMACPrefix: string,
}

export interface BLEManufacturerDetection {
    type: DetectionType.ble_id,
    mac: number[],
    id: number,
    idName: string | undefined,
}

export type DeviceEvent = WifiDeviceSignal | BluetoothDeviceSignal;
export type FlockDetectionEvent = NameDetection | SSIDDetection | MACDetection | BLEManufacturerDetection;

export interface RssiRecording {
    rssi: number,
    location: GeolocationPosition | undefined,
    timestamp: number,
}

class Summary {
    public mac: number[];
    public lastSeen: number;
    public rssis: RssiRecording[];
    public detections: FlockDetectionEvent[];
    public type: DeviceType;

    constructor(event: DeviceEvent) {
        this.mac = $state(event.mac);
        this.type = $state(event.type);
        this.lastSeen = $state(event.timestamp);
        this.detections = $state([]);
        this.rssis = $state([{
            rssi: event.rssi,
            location: event.location,
            timestamp: event.timestamp,
        }]);
    }

    update(event: DeviceEvent) {
        this.lastSeen = event.timestamp;
        this.rssis.push({
            rssi: event.rssi,
            location: event.location,
            timestamp: event.timestamp,
        });
    }

    public macToString(): string {
        return this.mac
            .map(n => n.toString(16).padStart(2, '0').toUpperCase())
            .join(':');
    }

    public lastRSSI(): RssiRecording {
        return this.rssis[this.rssis.length - 1];
    }

    public getOUIManufacturer(): string | undefined {
        const ouiPrefix = this.mac
            .slice(0, 3)
            .map(n => n.toString(16).padStart(2, '0').toUpperCase())
            .join('');
        if (ouiPrefix in ouiData) {
            const ouiResult = ouiData[ouiPrefix as keyof typeof ouiData];
            return ouiResult.split('\n')[0];
        }
        return undefined;
    }
}

export class WifiSummary extends Summary {
    public ssid: string;
    public frameType: WifiFrameType;

    constructor(event: WifiDeviceSignal) {
        super(event);
        this.ssid = $state(event.ssid);
        this.frameType = $state(event.frameType == 0x80 ? WifiFrameType.beacon : WifiFrameType.probe);
    }

    public static fromObj(obj: any): WifiSummary {
        const numRSSIs = obj['rssis'].length;
        const lastLoc: RssiRecording = obj['rssis'][numRSSIs - 1];
        const fakeSignal: WifiDeviceSignal = {
            type: DeviceType.wifi,
            timestamp: lastLoc.timestamp,
            location: lastLoc.location,
            rssi: lastLoc.rssi,
            ssid: obj['ssid'],
            channel: 0, // unused?
            mac: obj['mac'],
            frameType: obj['frameType'],
        };
        return new WifiSummary(fakeSignal);
    }
}

export class BLESummary extends Summary {
    public deviceName: string;
    manufacturerData: BLEManufacturerData[];

    constructor(event: BluetoothDeviceSignal) {
        super(event);
        this.deviceName = $state(event.deviceName);
        this.manufacturerData = $state(event.manufacturerData);
    }

    public getBLEManufacturerNames(): string[] {
        const unique: Set<string> = new Set();
        for (const data of this.manufacturerData) {
            if (data.idName) {
                unique.add(data.idName);
            } else {
                const hex = data.id.toString(16).padStart(4, '0');
                unique.add(`Unknown (${hex})`);
            }
        }
        return unique.values().toArray();
    }

    public static fromObj(obj: any): BLESummary {
        const numRSSIs = obj['rssis'].length;
        const lastLoc: RssiRecording = obj['rssis'][numRSSIs - 1];
        const fakeSignal: BluetoothDeviceSignal = {
            type: DeviceType.ble,
            location: lastLoc.location,
            timestamp: lastLoc.timestamp,
            rssi: lastLoc.rssi,
            mac: obj['mac'],
            deviceName: obj['deviceName'],
            manufacturerData: obj['manufacturerData'],
        };
        return new BLESummary(fakeSignal);
    }
}

function objectifySummary(summary: DeviceSummary): { [key: string]: any } {
    let result: { [key: string]: any } = {
        mac: summary.mac,
        lastSeen: summary.lastSeen,
        rssis: summary.rssis,
        detections: summary.detections,
        type: summary.type,
    };
    if ('deviceName' in summary) {
        result.deviceName = summary.deviceName;
        result.manufacturerData = summary.manufacturerData;
    } else {
        result.ssid = summary.ssid;
        result.frameType = summary.frameType;
    }
    return result;
}

export type DeviceSummary = WifiSummary | BLESummary;

export class ScanResults {
    public bles: Map<string, BLESummary> = new Map();
    public wifis: Map<string, WifiSummary> = new Map();
    public scanStartedTimestamp: number;
    public lastEventTimestamp: number | undefined;

    constructor() {
        this.scanStartedTimestamp = getNow();
    }

    appendDeviceEvent(event: DeviceEvent) {
        const macString = `${event.mac}`;
        if (event.type === DeviceType.ble) {
            let wipBLE = this.bles.get(macString);
            if (!wipBLE) {
                wipBLE = new BLESummary(event);
                this.bles.set(macString, wipBLE);
            } else {
                wipBLE.update(event);
            }
        } else if (event.type === DeviceType.wifi) {
            let wipWifi = this.wifis.get(macString);
            if (!wipWifi) {
                wipWifi = new WifiSummary(event);
                this.wifis.set(macString, wipWifi);
            } else {
                wipWifi.update(event);
            }
        }
        this.lastEventTimestamp = getNow();
    }

    appendDetectionEvent(event: FlockDetectionEvent) {
        const macString = `${event.mac}`;
        if (this.bles.has(macString)) {
            this.bles.get(macString)!.detections.push(event);
        } else if (this.wifis.has(macString)) {
            this.wifis.get(macString)!.detections.push(event);
        } else {
            console.warn(`failed to add detection event ${event}, no matching mac addr`);
            return;
        }
        this.lastEventTimestamp = getNow();
    }

    public toJSON(): string {
        return JSON.stringify({
            'ble': this.bles.values().map(objectifySummary).toArray(),
            'wifi': this.wifis.values().map(objectifySummary).toArray(),
            'scanStartedTimestamp': this.scanStartedTimestamp,
            'lastEventTimestamp': this.lastEventTimestamp,
        });
    }

    public static fromJSON(input: string): ScanResults {
        const obj = JSON.parse(input);
        if ('ble' in obj && 'wifi' in obj) {
            let result = new ScanResults();
            result.lastEventTimestamp = obj['lastEventTimestamp'];
            result.scanStartedTimestamp = obj['scanStartedTimestamp'];
            for (let bleObj of obj['ble']) {
                const ble = BLESummary.fromObj(bleObj);
                result.bles.set(`${ble.mac}`, ble);
            }
            for (let wifiObj of obj['wifi']) {
                const wifi = WifiSummary.fromObj(wifiObj);
                result.wifis.set(`${wifi.mac}`, wifi);
            }
            return result;
        } else {
            throw new Error(`invalid JSON input "${input}"`);
        }
    }

    public getDevices(
        macs: string[],
        pinnedMACs: string[],
        rssiThreshold: number,
    ): [DeviceSummary[], DeviceSummary[], DeviceSummary[]] {
        let result = [];
        let pinnedResult = [];
        let detectedResult = [];
        for (const [mac, ble] of this.bles) {
            if (macs.includes(mac) && ble.lastRSSI().rssi > rssiThreshold) {
                result.push(ble);
            }
            if (ble.detections.length > 0) {
                detectedResult.push(ble);
            }
        }
        for (const [mac, wifi] of this.wifis) {
            if (macs.includes(mac) && wifi.lastRSSI().rssi > rssiThreshold) {
                result.push(wifi);
            }
            if (wifi.detections.length > 0) {
                detectedResult.push(wifi);
            }
        }
        for (const mac of pinnedMACs) {
            if (this.bles.has(mac)) {
                pinnedResult.push(this.bles.get(mac)!);
            } else if (this.wifis.has(mac)) {
                pinnedResult.push(this.wifis.get(mac)!);
            }
        }
        result.sort((a, b) => {
            return b.lastRSSI().rssi - a.lastRSSI().rssi;
        });
        return [result, pinnedResult, detectedResult];
    }
}

function getNow(): number {
    return (new Date()).getTime();
}

export enum ScannerStatus {
    disconnected = 'disconnected',
    connected = 'connected',
}

export class Scanner {
    public LAST_SEEN_THRESHOLD_MS = 10_000;
    public errors: string[] = $state([]);
    public currentLocation: GeolocationPosition | undefined = $state(undefined);
    public results = new ScanResults();
    public status: ScannerStatus = $state(ScannerStatus.disconnected);

    private lastSeenTimestamp: Map<string, number> = new Map();

    constructor(private recordingStore: RecordingStore) {
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
            return;
        }
        return new Promise((success, failure) => {
            navigator.geolocation.watchPosition((location) => {
                this.currentLocation = location;
                success();
            }, (err) => {
                failure(err);
            });
        });
    }

    public static async setupDummy() {
        const store = await RecordingStore.open();
        const result = new Scanner(store);
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
        const characteristicUUID = 0x0001;
        const device = await navigator.bluetooth.requestDevice({filters: [{ name: "FlockYou", services: [serviceUUIDAlias] }], optionalServices: [serviceUUID] });
        if (!device.gatt) {
            throw new Error(`device has no GATT`);
        }
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(serviceUUID);
        const characteristic = await service.getCharacteristic(characteristicUUID);
        await characteristic.startNotifications();
        const store = await RecordingStore.open();
        const result = new Scanner(store);
        characteristic.addEventListener('characteristicvaluechanged', async (_: Event) => {
            try {
                result.processEvent(characteristic.value!);
            } catch (err) {
                result.errors.push(`${err}`);
            }
        });
        setInterval(() => {
            if (!server.connected) {
                result.status = ScannerStatus.disconnected;
            }
        })
        await result.watchLocation();
        return result;
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
            const mac: number[] = array[1];
            const detectionType: string = array[2];
            if (detectionType === 'mac') {
                this.appendDetectionEvent({
                    type: DetectionType.mac,
                    mac,
                    matchedMACPrefix: array[3],
                });
            } else if (detectionType === 'name') {
                this.appendDetectionEvent({
                    type: DetectionType.name,
                    mac,
                    name: array[3],
                    matchedName: array[4],
                });
            } else if (detectionType === 'ssid') {
                this.appendDetectionEvent({
                    type: DetectionType.ssid,
                    mac,
                    ssid: array[3],
                    matchedSSID: array[4],
                });
            } else if (detectionType === 'ble_id') {
                this.appendDetectionEvent({
                    type: DetectionType.ble_id,
                    mac,
                    id: array[3],
                    idName: lookupManufacturerIDName(array[3]),
                })
            } else {
                throw new Error(`invalid detection event: ${array}`);
            }
        } else if (eventType === "data_too_large") {
            throw new Error("data payload too large");
        } else {
            throw new Error(`invalid scan event: "${array}"`)
        }
    }
}
