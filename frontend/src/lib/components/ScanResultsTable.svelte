<script lang="ts">
    import { DeviceType, Scanner, WifiFrameType, type DeviceSummary } from "$lib/scanResults.svelte";
    import { onMount } from "svelte";
    import DeviceSummaryCard from "./DeviceSummaryCard.svelte";
    import RssiThresholdControl from "./RssiThresholdControl.svelte";
    import DeviceTypeFilterControl from "./DeviceTypeFilterControl.svelte";
    import DeviceModal from "./DeviceModal.svelte";

    const TIME_THRESHOLD_MS = 10_000;
    const { scanResults }: { scanResults: Scanner } = $props();
    let inRangeDevices: DeviceSummary[] = $state([]);
    let pinnedDevices: DeviceSummary[] = $state([]);
    let pinnedMACs: string[] = $state([]);
    let detectedDevices: DeviceSummary[] = $state([]);
    let rssiThreshold = $state(-60);
    let showWifiBeacon = $state(true);
    let showWifiProbe = $state(true);
    let showBLE = $state(true);
    let currentLocation: GeolocationCoordinates | undefined = $state(undefined);

    let selectedDevice: DeviceSummary | undefined = $state(undefined);
    let showModal = $state(true);

    function onPin(device: DeviceSummary) {
        const mac = `${device.mac}`;
        pinnedDevices.push(device);
        if (!pinnedMACs.includes(mac)) {
            pinnedMACs.push(mac);
        }
    }

    function onUnpin(needle: DeviceSummary) {
        const macString = `${needle.mac}`;
        pinnedDevices = pinnedDevices.filter(haystack => `${haystack.mac}` !== macString)
        pinnedMACs = pinnedMACs.filter(haystack => haystack !== macString);
    }

    function updateDevices() {
        try {
            [
                inRangeDevices,
                pinnedDevices,
                detectedDevices,
            ] = scanResults.summarize(
                TIME_THRESHOLD_MS,
                rssiThreshold,
                pinnedMACs,
            );
            currentLocation = scanResults.currentLocation!.coords;
        } catch(e) {
            scanResults.errors.push(JSON.stringify(e));
        }
    }

    function showDevice(device: DeviceSummary) {
        const isWifiBeacon = 'ssid' in device && device.frameType === WifiFrameType.beacon;
        const isWifiProbe = 'ssid' in device && device.frameType === WifiFrameType.probe;
        if (!showBLE && device.type === DeviceType.ble) {
            return false;
        } else if (!showWifiBeacon && isWifiBeacon) {
            return false;
        } else if (!showWifiProbe && isWifiProbe) {
            return false;
        }
        return true;
    }

    function download() {
        const text = scanResults.getJSON();
        const element = document.createElement('a');
        element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
        element.setAttribute('download', 'flockYouData.json');

        element.style.display = 'none';
        document.body.appendChild(element);

        element.click();

        document.body.removeChild(element);
    }

    function onSelect(device: DeviceSummary) {
        selectedDevice = device;
        showModal = true;
    }

    let interval: number;
    onMount(() => {
        interval = setInterval(updateDevices, 250);
    });
</script>

<div class="m-3">
    <pre>{JSON.stringify(scanResults.errors)}</pre>
    <div>
        <RssiThresholdControl bind:rssiThreshold />
        <DeviceTypeFilterControl bind:showBLE bind:showWifiBeacon bind:showWifiProbe />
        <button class="bg-blue-700 hover:bg-blue-800 px-5 py-2.5 me-2 mb-2 text-white rounded-md" onclick={download}>Download Data as JSON</button>
    </div>
    <div>
        <p>Detected Flock Devices</p>
        <div class="flex flex-wrap w-4/5">
            {#each detectedDevices as device}
                <DeviceSummaryCard {device} {onSelect} />
            {/each}
        </div>
        <p>Pinned</p>
        <div class="flex flex-wrap w-4/5">
            {#each pinnedDevices as device}
                {#if showDevice(device)}
                    <DeviceSummaryCard {device} {onUnpin} {onSelect} />
                {/if}
            {/each}
        </div>
        <p>In-Range:</p>
        <div class="flex flex-wrap w-4/5">
            {#each inRangeDevices as device}
                {#if showDevice(device)}
                    {#if pinnedMACs.includes(`${device.mac}`) }
                        <DeviceSummaryCard {device} {onSelect} />
                    {:else}
                        <DeviceSummaryCard {device} {onPin} {onSelect} />
                    {/if}
                {/if}
            {/each}
        </div>
    </div>
    <DeviceModal device={selectedDevice} currentLocation={currentLocation!} bind:showModal />
</div>
