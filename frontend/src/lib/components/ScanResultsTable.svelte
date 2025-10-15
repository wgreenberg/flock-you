<script lang="ts">
    import { ScanResults } from "$lib/scanResults.svelte";
    import BLESummary from "./BLESummary.svelte";
    import WifiSummary from "./WifiSummary.svelte";

    const { scanResults }: { scanResults: ScanResults } = $props();
    let { inRangeWifis, inRangeBLEs } = $derived(scanResults.summarize(scanResults.events));
</script>

<div>
    <pre>{JSON.stringify(scanResults.errors)}</pre>
    <div>
        <p>In-Range:</p>
        <div class="flex">
            {#each inRangeWifis as wifi}
                <WifiSummary {wifi} />
            {/each}
            {#each inRangeBLEs as ble}
                <BLESummary {ble} />
            {/each}
        </div>
    </div>
    <ul>
        {#each scanResults.events as event}
            <li><pre>{JSON.stringify(event)}</pre></li>
        {/each}
    </ul>
</div>
