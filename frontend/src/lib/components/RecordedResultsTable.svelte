<script lang="ts">
    import { ScanResults, type DeviceSummary } from "$lib/scanResults.svelte";
    import { onMount } from "svelte";
    import DeviceSummaryCard from "./DeviceSummaryCard.svelte";
    import DeviceModal from "./DeviceModal.svelte";

    let results: ScanResults[] = $state([]);
    let selectedIndex: number | undefined = $state(undefined);
    let selectedDevice: DeviceSummary | undefined = $state(undefined);
    let showModal = $state(false);

    onMount(() => {
        for (let i=0; i<window.localStorage.length; i++) {
            try {
                const timestampStr = window.localStorage.key(i)!;
                results.push(ScanResults.fromJSON(window.localStorage.getItem(timestampStr)!));
            } catch(err) {
                console.error(err);
                console.error(`failed to load result ${i}: ${err}`);
            }
        }
    });

    function deleteRecording(index: number) {
        const key = window.localStorage.key(index);
        if (key === null) {
            throw new Error(`failed to delete recording with index ${index}`);
        }
        if (index === selectedIndex) {
            selectedIndex = undefined;
        }
        window.localStorage.removeItem(key);
        results = results.filter((_, i) => i !== index);
    }

    function onSelect(device: DeviceSummary) {
        selectedDevice = device;
        showModal = true;
    }

    function getDuration(result: ScanResults): string {
        if (result.lastEventTimestamp === undefined) {
            return "N/A";
        }
        const seconds = (result.lastEventTimestamp - result.scanStartedTimestamp) / 1000;
        if (seconds < 60) {
            return `${seconds.toPrecision(2)}s`;
        } else {
            return `${Math.floor(seconds / 60)}h ${(seconds % 60).toPrecision(2)}s`
        }
    }
</script>

<div>
    <ul>
        {#each results as result, i}
            {@const scanStarted = new Date(result.scanStartedTimestamp)}
            {@const durationMS = result.lastEventTimestamp ? result.lastEventTimestamp - result.scanStartedTimestamp : undefined}
            {@const numDevices = result.bles.size + result.wifis.size}
            <li class="flex flex-row m-1">
                <button class="p-2 rounded-md rounded-r-none mr-0 bg-blue-500 hover:bg-blue-600 text-white" onclick={() => selectedIndex = i}>
                    {scanStarted.toLocaleString()} (duration {getDuration(result)}, {numDevices} devices)
                </button>
                <button class="p-2 rounded-md rounded-l-none ml-0 bg-red-500 hover:bg-red-600 text-white" onclick={() => deleteRecording(i)}>
                    Delete
                </button>
            </li>
        {/each}
    </ul>
    <div class="flex flex-wrap w-4/5">
        {#if selectedIndex !== undefined}
            {@const device = results[selectedIndex]}
            {#each device.bles as [mac, ble]}
                <DeviceSummaryCard {onSelect} device={ble} />
            {/each}
            {#each device.wifis as [mac, wifi]}
                <DeviceSummaryCard {onSelect} device={wifi} />
            {/each}
        {/if}
    </div>
</div>
<DeviceModal device={selectedDevice} bind:showModal />
