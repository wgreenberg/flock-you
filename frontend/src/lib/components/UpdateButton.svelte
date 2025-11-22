<script lang="ts">
    import { CATEGORIES } from "$lib/scanCategories";
    import { ProgressTracker, type Scanner } from "$lib/scanner.svelte";

    let { scanner }: {
        scanner: Scanner | undefined,
    } = $props();

    let progress = new ProgressTracker();
    function updateCategories() {
        if (scanner) {
            scanner.updateScanCategories(CATEGORIES, progress);
        }
    }
</script>

<div>
    <button onclick={updateCategories}>
        <span>Update</span>
        {#if progress.total !== undefined && progress.soFar !== undefined}
            <span>{100 * progress.soFar / progress.total}</span>
        {/if}
    </button>
</div>
