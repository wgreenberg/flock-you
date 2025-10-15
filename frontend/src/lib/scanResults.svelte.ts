import { decode } from "@msgpack/msgpack";

export enum EventType {
    wifi = 'wifi',
    ble = 'ble',
    outOfRange = 'outOfRange',
}

export interface OutOfRangeEvent {
    type: EventType.outOfRange,
}

export interface BluetoothEvent {
    type: EventType.ble,
    location: GeolocationPosition | undefined,
    timestamp: number,
    rssi: number,
    mac_address: string,
    device_name: string | undefined,
    matched_name: string | undefined,
    matched_mac: string | undefined,
}

export interface WifiEvent {
    type: EventType.wifi,
    timestamp: number,
    location: GeolocationPosition | undefined,
    ssid: string,
    rssi: number,
    channel: number,
    mac_address: string,
    matched_ssid: string | undefined,
    matched_mac: string | undefined,
    frame_type: number,
}

export type ScanEvent = WifiEvent | BluetoothEvent | OutOfRangeEvent;

export interface RssiRecording {
    rssi: number,
    location: GeolocationPosition | undefined,
    timestamp: number,
}

export interface WifiSummary {
    ssid: string,
    mac_address: string,
    matched_ssid: string | undefined,
    matched_mac: string | undefined,
    rssis: RssiRecording[],
}

export interface BLESummary {
    mac_address: string,
    device_name: string | undefined,
    matched_name: string | undefined,
    matched_mac: string | undefined,
    rssis: RssiRecording[],
}

export interface ScanResultSummary {
    inRangeWifis: WifiSummary[],
    inRangeBLEs: BLESummary[],
}

export class ScanResults {
    public events: ScanEvent[] = $state([]);
    public errors: string[] = $state([]);
    public currentLocation: GeolocationPosition | undefined = $state(undefined);

    public summarize(events: ScanEvent[]): ScanResultSummary {
        let lastOutOfRange = events.findLastIndex(e => e.type === EventType.outOfRange);
        let inRangeEvents = events;
        let inRangeWifis: Map<string, WifiSummary> = new Map();
        let inRangeBLEs: Map<string, BLESummary> = new Map();
        if (lastOutOfRange >= 0) {
            inRangeEvents = events.slice(lastOutOfRange);
        }
        for (const event of inRangeEvents) {
            if (event.type === EventType.ble) {
                let summary = inRangeBLEs.get(event.mac_address);
                let rssi = {
                    rssi: event.rssi,
                    location: event.location,
                    timestamp: event.timestamp,
                };
                if (summary) {
                    summary.rssis.push(rssi);
                } else {
                    inRangeBLEs.set(event.mac_address, {
                        device_name: event.device_name,
                        mac_address: event.mac_address,
                        matched_name: event.matched_name,
                        matched_mac: event.matched_mac,
                        rssis: [rssi],
                    });
                }
            } else if (event.type === EventType.wifi) {
                let summary = inRangeWifis.get(event.mac_address);
                let rssi = {
                    rssi: event.rssi,
                    location: event.location,
                    timestamp: event.timestamp,
                };
                if (summary) {
                    summary.rssis.push(rssi);
                } else {
                    inRangeWifis.set(event.mac_address, {
                        ssid: event.ssid,
                        mac_address: event.mac_address,
                        matched_ssid: event.matched_ssid,
                        matched_mac: event.matched_mac,
                        rssis: [rssi],
                    });
                }
            }
        }
        return {
            inRangeWifis: inRangeWifis.values().toArray(),
            inRangeBLEs: inRangeBLEs.values().toArray(),
        };
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
        const result = new ScanResults();
        setInterval(async () => {
            const location = result.currentLocation;
            const timestamp = (new Date()).getTime()
            const rng = Math.random();
            if (rng < 0.1) {
                result.events.push({
                    type: EventType.outOfRange,
                });
            } else if (rng < 0.6) {
                result.events.push({
                    type: EventType.ble,
                    location,
                    timestamp,
                    rssi: -Math.random() * 100,
                    mac_address: 'de:ad:be:ef:de:ad:be:ef',
                    device_name: 'whatever',
                    matched_name: 'whatever man',
                    matched_mac: undefined,
                });
            } else {
                result.events.push({
                    type: EventType.wifi,
                    location,
                    timestamp,
                    ssid: 'femboy hooters',
                    rssi: -Math.random() * 100,
                    channel: 10,
                    mac_address: 'de:ad:be:ef:de:ad:be:ef',
                    matched_ssid: undefined,
                    matched_mac: 'de:ad:be:ef:de:ad:be:ef',
                    frame_type: 20,
                });
            }
        }, 1000);
        await result.watchLocation();
        return result;
    }

    public static async setupFromBLEDevice(): Promise<ScanResults> {
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
        const result = new ScanResults();
        characteristic.addEventListener('characteristicvaluechanged', async (_: Event) => {
            try {
                result.events.push(result.parseScanEvent(characteristic.value!));
            } catch (err) {
                result.errors.push(`${err}`);
            }
        });
        await result.watchLocation();
        return result;
    }

    parseScanEvent(data: DataView): ScanEvent {
        console.log(data);
        const array: any[] = decode(data) as any[];
        const location = this.currentLocation;
        const timestamp = (new Date()).getTime();
        const eventType = array[0];
        if (eventType === 'bluetooth_le') {
            return {
                type: EventType.ble,
                location,
                timestamp,
                device_name: array[1],
                matched_name: array[2],
                rssi: array[3],
                mac_address: array[4],
                matched_mac: array[5],
            };
        } else if (eventType === 'wifi') {
            return {
                type: EventType.wifi,
                location,
                timestamp,
                rssi: array[1],
                ssid: array[2],
                matched_ssid: array[3],
                channel: array[4],
                mac_address: array[5],
                matched_mac: array[6],
                frame_type: array[7],
            };
        } else if (eventType === "out_of_range") {
            return {
                type: EventType.outOfRange,
            };
        } else if (eventType === "data_too_large") {
            throw new Error("data payload too large");
        } else {
            throw new Error(`invalid scan event: "${array}"`)
        }
    }
}
