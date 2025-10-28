<script>
	let { showModal = $bindable(), children } = $props();

	let dialog = $state(); // HTMLDialogElement

	$effect(() => {
		if (showModal) dialog.showModal();
	});
</script>

<!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_noninteractive_element_interactions -->
<dialog
    class="show bg-white rounded-lg shadow-lg p-6 w-11/12 mt-2 ml-2 max-w-lg"
   	bind:this={dialog}
   	onclose={() => (showModal = false)}
   	onclick={(e) => { if (e.target === dialog) dialog.close(); }}
>
   	<div>
  		{@render children?.()}
  		<button
            class="absolute top-2 right-2"
            onclick={() => dialog.close()}>&#x2715;</button>
   	</div>
</dialog>

<style>
	dialog::backdrop {
		background: rgba(0, 0, 0, 0.3);
	}
	dialog[open] {
		animation: zoom 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
	}
	@keyframes zoom {
		from {
			transform: scale(0.95);
		}
		to {
			transform: scale(1);
		}
	}
	dialog[open]::backdrop {
		animation: fade 0.2s ease-out;
	}
	@keyframes fade {
		from {
			opacity: 0;
		}
		to {
			opacity: 1;
		}
	}
</style>
