<script lang="ts">
    import { Scanner, type DeviceSummary } from "$lib/scanResults.svelte";
    import { onMount } from "svelte";
    import DeviceSummaryCard from "./DeviceSummaryCard.svelte";

    const { scanResults }: { scanResults: Scanner } = $props();
    let inRangeDevices: DeviceSummary[] = $state([]);
    let pinnedDevices: DeviceSummary[] = $state([]);
    let pinnedMACs: string[] = $state([]);

    function onPin(device: DeviceSummary) {
        const mac = `${device.mac}`;
        if (!pinnedMACs.includes(mac)) {
            pinnedMACs.push(mac);
        }
    }

    function onUnpin(needle: DeviceSummary) {
        pinnedMACs = pinnedMACs.filter(haystack => haystack !== `${needle.mac}`);
    }

    let interval: number;
    onMount(() => {
        interval = setInterval(() => {
            try {
                [inRangeDevices, pinnedDevices] = scanResults.summarize(10_000, -200, pinnedMACs);
            } catch(e) {
                alert(e);
            }
        }, 1000);
    });
</script>

<div>
    <pre>{JSON.stringify(scanResults.errors)}</pre>
    <div>
        <p>Pinned</p>
        <div class="flex flex-wrap w-4/5">
            {#each pinnedDevices as device}
                <DeviceSummaryCard {device} {onUnpin} />
            {/each}
        </div>
        <p>In-Range:</p>
        <div class="flex flex-wrap w-4/5">
            {#each inRangeDevices as device}
                <DeviceSummaryCard {device} {onPin} />
            {/each}
        </div>
    </div>
</div>
