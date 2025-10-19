<script lang="ts">
    import { DeviceType, WifiFrameType, type DeviceSummary } from "$lib/scanResults.svelte";
    import DeviceSummaryItems from "./DeviceSummaryItems.svelte";
    import SummaryIcon from "./SummaryIcon.svelte";

    let { device, onPin, onUnpin, onSelect }: {
        device: DeviceSummary,
        onPin?: (device: DeviceSummary) => void,
        onUnpin?: (device: DeviceSummary) => void,
        onSelect?: (device: DeviceSummary) => void,
    } = $props();

    // awkward svelte/tailwind nonsense to get colors updating dynamically
    let cardColor = $derived(
        device.detections.length > 0 ?
            device.type === DeviceType.wifi ? 'bg-red-300': 'bg-red-400' :
            device.type === DeviceType.wifi ? 'bg-fuchsia-100': 'bg-fuchsia-200'
    );
    let cardClass = $derived("rounded-lg m-2 p-2 flex flex-col w-78 " + cardColor);

    function onClickPin () {
        if (onPin) {
            onPin(device);
        } else if (onUnpin) {
            onUnpin(device);
        }
    }

    function onClickCard() {
        if (onSelect) {
            onSelect(device);
        }
    }
</script>

<div class={cardClass}>
    <span class="flex justify-between">
        <button onclick={onClickCard}>
            <div class="flex flex-row align-bottom">
                {#if 'ssid' in device}
                    <SummaryIcon
                        type={device.frameType == WifiFrameType.beacon ? "wifiBeacon" : "wifiProbe"}
                        label={`Wifi ${device.frameType === WifiFrameType.beacon ? "Beacon" : "Probe Request"}`}
                        />
                {:else}
                    <SummaryIcon
                        type="ble"
                        label="Bluetooth LE"
                        />
                {/if}
            </div>
        </button>
        {#if onPin || onUnpin}
        <button onclick={onClickPin}>
            <SummaryIcon type={onPin ? 'pin' : 'unpin'} />
        </button>
        {/if}
    </span>
    <DeviceSummaryItems {device} />
</div>
