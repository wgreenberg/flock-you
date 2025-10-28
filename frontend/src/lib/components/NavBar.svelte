<script lang="ts">
    import type { Scanner } from "$lib/scanResults.svelte";

    let { selection = $bindable(), scanner }: {
        selection: string,
        scanner: Scanner | undefined,
    } = $props();
    let selectionOptions = [
        { selection: 'scanner', text: 'Scanner' },
        { selection: 'recordedResults', text: 'Recorded Results' },
    ];
</script>

<nav class="items-center justify-between p-6 bg-gray-800/50 after:pointer-events-none after:absolute after:inset-x-0 after:bottom-0 after:h-px after:bg-white/10">
    <div class="w-full block flex flex-row justify-between flex-grow">
        <div class="text-sm flex flex-row">
            {#each selectionOptions as item}
                {#if item.selection === selection}
                    <button onclick={() => selection = item.selection} class="rounded-md bg-gray-950/50 px-3 py-2 text-sm font-medium text-white">
                        {item.text}
                    </button>
                {:else}
                    <button onclick={() => selection = item.selection} class="rounded-md px-3 py-2 text-sm font-medium text-gray-300 hover:bg-white/5 hover:text-white">
                        {item.text}
                    </button>
                {/if}
            {/each}
        </div>
        <div></div>
        <div>
            <span>Status: {scanner ? scanner.status : 'disconnected' }</span>
        </div>
    </div>
</nav>
