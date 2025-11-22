import ouiData from "oui-data";
import { getNow } from "./utils";

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
    ble_name = 'ble_name',
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

export type DeviceEvent = WifiDeviceSignal | BluetoothDeviceSignal;
export type FlockDetectionEvent = {
    type: DetectionType,
    category: string,
    mac: number[],
};

export interface RssiRecording {
    rssi: number,
    location?: GeolocationPosition,
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
