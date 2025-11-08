import { ScanResults } from "./scanResults.svelte";

function promisifyReq<T>(req: IDBRequest<T>): Promise<T> {
    return new Promise((resolve, reject) => {
        req.onsuccess = () => resolve(req.result);
        req.onerror = (event) => {
            reject(req.error);
        };
    })
}

export class RecordingStore {
    static DB_NAME = 'flock-you';
    static OBJECT_STORE_NAME = 'recordings';

    constructor(public db: IDBDatabase) {
    }

    public async loadResults(timestamp: string): Promise<ScanResults> {
        const store = this.db.transaction(RecordingStore.OBJECT_STORE_NAME, 'readonly')
            .objectStore(RecordingStore.OBJECT_STORE_NAME);
        const result: string = await promisifyReq(store.get(timestamp));
        return ScanResults.fromJSON(result);
    }

    public async listTimestamps(): Promise<string[]> {
        const store = this.db.transaction(RecordingStore.OBJECT_STORE_NAME, 'readonly')
            .objectStore(RecordingStore.OBJECT_STORE_NAME);
        const keys = await promisifyReq(store.getAllKeys());
        return keys.map(key => key.toString());
    }

    public async updateResults(results: ScanResults) {
        const timestamp = `${results.scanStartedTimestamp}`;
        const store = this.db.transaction(RecordingStore.OBJECT_STORE_NAME, 'readwrite')
            .objectStore(RecordingStore.OBJECT_STORE_NAME);
        await promisifyReq(store.put(results.toJSON(), timestamp));
    }

    public async deleteResults(results: ScanResults) {
        const timestamp = `${results.scanStartedTimestamp}`;
        const store = this.db.transaction(RecordingStore.OBJECT_STORE_NAME, 'readwrite')
            .objectStore(RecordingStore.OBJECT_STORE_NAME);
        await promisifyReq(store.delete(timestamp));
    }

    public static async open(): Promise<RecordingStore> {
        const dbReq = window.indexedDB.open(RecordingStore.DB_NAME, 1);
        return new Promise((resolve, reject) => {
            dbReq.onsuccess = () => {
                const db = dbReq.result;
                resolve(new RecordingStore(db));
            };
            dbReq.onerror = () => {
                reject(new Error(`failed to load DB`));
            };
            dbReq.onupgradeneeded = () => {
                const db = dbReq.result;
                const objectStore = db.createObjectStore(RecordingStore.OBJECT_STORE_NAME);
            };
        });
    }
}
