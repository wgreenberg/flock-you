<script lang="ts">
    import { DeviceType, WifiFrameType, type DeviceSummary } from "$lib/scanResults.svelte";
    import SummaryIcon from "./SummaryIcon.svelte";

    let { device, onPin = undefined, onUnpin = undefined }: {
        device: DeviceSummary,
        onPin?: (device: DeviceSummary) => void,
        onUnpin?: (device: DeviceSummary) => void,
    } = $props();

    let isMacRandomized = $derived([0x2, 0x6, 0xA, 0xE].includes(device.mac[0] & 0x0F));

    // awkward svelte/tailwind nonsense to get colors updating dynamically
    let cardColor = $derived(device.type === DeviceType.wifi ? 'bg-fuchsia-100': 'bg-fuchsia-200' );
    let cardClass = $derived("rounded-lg m-2 p-2 flex flex-col w-48 " + cardColor);

    function onclick () {
        if (onPin) {
            onPin(device);
        } else if (onUnpin) {
            onUnpin(device);
        }
    }
</script>

<div class={cardClass}>
    {#if 'ssid' in device}
        <span class="flex">
            <SummaryIcon type={device.frameType == WifiFrameType.beacon ? "wifiBeacon" : "wifiProbe"} />
            <h3>Wifi</h3>
            <button {onclick}>
                <SummaryIcon type={onPin ? 'pin' : 'unpin'} />
            </button>
        </span>
        <span>SSID: {device.ssid ? device.ssid : "N/A"}</span>
        <span>
            MAC Address: {device.macToString()}
            {#if isMacRandomized}
                <SummaryIcon type="dice" />
            {/if}
        </span>
        <span>RSSI: {device.lastRSSI().rssi}</span>
    {:else}
        <span class="flex flex-row">
            <SummaryIcon type="ble" />
            <h3>Bluetooth LE</h3>
            <button {onclick}>
                <SummaryIcon type={onPin ? 'pin' : 'unpin'} />
            </button>
        </span>
        <span>Name: {device.deviceName ? device.deviceName : "N/A"}</span>
        <span>
            MAC Address: {device.macToString()}
            {#if isMacRandomized}
                <SummaryIcon type="dice" />
            {/if}
        </span>
        <span>RSSI: {device.lastRSSI().rssi}</span>
    {/if}
</div>
