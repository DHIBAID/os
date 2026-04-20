#include "tests/test_memory.h"

#include "lib/printf.h"

static int g_total_tests = 0;
static int g_passed_tests = 0;

static void print_result(const char* test_name, int pass) {
	g_total_tests++;
	if (pass) {
		g_passed_tests++;
	}

	print_str(pass ? "[PASS] " : "[FAIL] ");
	print_str(test_name);
	print_str("\n");
}

static void print_step(const char* message) {
	print_str("  - ");
	print_str(message);
	print_str("\n");
}

static void print_value_dec(const char* label, uint64_t value) {
	print_str("    ");
	print_str(label);
	print_str(": ");
	print_dec(value);
	print_str("\n");
}

static void print_value_hex(const char* label, uint64_t value) {
	print_str("    ");
	print_str(label);
	print_str(": ");
	print_hex(value);
	print_str("\n");
}

static int run_pmm_basic(void) {
	print_step("PMM: capture free frame count before allocations");
	uint64_t free_before = pmm_free_frames();
	print_value_dec("free_before", free_before);

	print_step("PMM: allocate two frames");
	void* f1 = pmm_alloc_frame();
	void* f2 = pmm_alloc_frame();
	print_value_hex("frame_1", (uint64_t)(uintptr_t)f1);
	print_value_hex("frame_2", (uint64_t)(uintptr_t)f2);

	if (!f1 || !f2 || f1 == f2) {
		print_step(
			"PMM: allocation validation failed (null or duplicate frame)");
		if (f1) pmm_free_frame(f1);
		if (f2) pmm_free_frame(f2);
		return 0;
	}

	print_step("PMM: verify free frame count decreased by exactly 2");
	uint64_t free_after_alloc = pmm_free_frames();
	print_value_dec("free_after_alloc", free_after_alloc);
	if (free_before < 2 || free_after_alloc + 2 != free_before) {
		print_step("PMM: frame accounting mismatch after allocations");
		pmm_free_frame(f1);
		pmm_free_frame(f2);
		return 0;
	}

	print_step("PMM: free both frames");
	pmm_free_frame(f1);
	pmm_free_frame(f2);

	uint64_t free_after_free = pmm_free_frames();
	print_value_dec("free_after_free", free_after_free);
	if (free_after_free != free_before) {
		print_step("PMM: free frame count did not return to initial value");
		return 0;
	}

	return 1;
}

static int run_heap_basic(void) {
	print_step("Heap: capture stats before allocations");
	HeapStats stats_before;
	HeapStats stats_after;

	kheap_get_stats(&stats_before);
	print_value_dec("used_before", stats_before.used_bytes);
	print_value_dec("free_before", stats_before.free_bytes);

	print_step("Heap: allocate 64-byte and 128-byte buffers");
	uint8_t* a = (uint8_t*)kmalloc(64);
	uint8_t* b = (uint8_t*)kmalloc(128);
	print_value_hex("ptr_a", (uint64_t)(uintptr_t)a);
	print_value_hex("ptr_b", (uint64_t)(uintptr_t)b);
	if (!a || !b || a == b) {
		print_step(
			"Heap: allocation validation failed (null or duplicate pointer)");
		if (a) kfree(a);
		if (b) kfree(b);
		return 0;
	}

	// 0xA0 and 0xB0 prefixes distinguish the two buffers so cross-buffer
	// corruption (e.g. from an off-by-one overwrite) is immediately visible.
	// The low nibble cycles 0-15 to catch boundary-alignment issues.
	print_step("Heap: write deterministic patterns to allocations");
	for (size_t i = 0; i < 64; i++) a[i] = (uint8_t)(0xA0 + (i & 0x0F));
	for (size_t i = 0; i < 128; i++) b[i] = (uint8_t)(0xB0 + (i & 0x0F));

	print_step("Heap: verify first buffer pattern");
	for (size_t i = 0; i < 64; i++) {
		if (a[i] != (uint8_t)(0xA0 + (i & 0x0F))) {
			print_step("Heap: data mismatch in first buffer");
			print_value_dec("failing_index", i);
			kfree(a);
			kfree(b);
			return 0;
		}
	}

	print_step("Heap: verify second buffer pattern");
	for (size_t i = 0; i < 128; i++) {
		if (b[i] != (uint8_t)(0xB0 + (i & 0x0F))) {
			print_step("Heap: data mismatch in second buffer");
			print_value_dec("failing_index", i);
			kfree(a);
			kfree(b);
			return 0;
		}
	}

	print_step("Heap: free both allocations");
	kfree(a);
	kfree(b);

	print_step("Heap: capture stats after free");
	kheap_get_stats(&stats_after);
	print_value_dec("used_after", stats_after.used_bytes);
	print_value_dec("free_after", stats_after.free_bytes);

	if (stats_after.used_bytes > stats_before.used_bytes) {
		print_step("Heap: used bytes grew after free operations");
		return 0;
	}

	if (stats_after.free_bytes < stats_before.free_bytes) {
		print_step("Heap: free bytes dropped below baseline after free");
		return 0;
	}

	return 1;
}

static int run_heap_calloc_realloc(void) {
	print_step("Heap: allocate zeroed buffer with kcalloc(16, 4)");
	uint8_t* p = (uint8_t*)kcalloc(16, 4);	// 64 bytes
	print_value_hex("ptr_p", (uint64_t)(uintptr_t)p);
	if (!p) {
		print_step("Heap: kcalloc returned null");
		return 0;
	}

	print_step("Heap: verify zero initialization and write sequence pattern");
	for (size_t i = 0; i < 64; i++) {
		if (p[i] != 0) {
			print_step("Heap: calloc region not zeroed");
			print_value_dec("failing_index", i);
			kfree(p);
			return 0;
		}
		p[i] = (uint8_t)(i + 1);
	}

	print_step("Heap: grow allocation with krealloc to 128 bytes");
	uint8_t* q = (uint8_t*)krealloc(p, 128);
	print_value_hex("ptr_q", (uint64_t)(uintptr_t)q);
	if (!q) {
		print_step("Heap: krealloc grow returned null");
		kfree(p);
		return 0;
	}

	print_step("Heap: verify preserved prefix after grow");
	for (size_t i = 0; i < 64; i++) {
		if (q[i] != (uint8_t)(i + 1)) {
			print_step("Heap: data mismatch after grow realloc");
			print_value_dec("failing_index", i);
			kfree(q);
			return 0;
		}
	}

	print_step("Heap: shrink allocation with krealloc to 32 bytes");
	uint8_t* r = (uint8_t*)krealloc(q, 32);
	print_value_hex("ptr_r", (uint64_t)(uintptr_t)r);
	if (!r) {
		print_step("Heap: krealloc shrink returned null");
		kfree(q);
		return 0;
	}

	print_step("Heap: verify preserved prefix after shrink");
	for (size_t i = 0; i < 32; i++) {
		if (r[i] != (uint8_t)(i + 1)) {
			print_step("Heap: data mismatch after shrink realloc");
			print_value_dec("failing_index", i);
			kfree(r);
			return 0;
		}
	}

	print_step("Heap: verify krealloc(ptr, 0) returns null and frees memory");
	if (krealloc(r, 0) != (void*)0) {
		print_step("Heap: krealloc(ptr, 0) did not return null");
		return 0;
	}

	return 1;
}

static int run_vmm_map_unmap(void) {
	// 0x51000000 = 1.28 GiB - above the 1 GiB identity-mapped by the bootloader,
	// so this address is guaranteed to be unmapped at test entry.
	const uint64_t test_virtual = 0x51000000ULL;

	print_step("VMM: verify test virtual address is not currently mapped");
	print_value_hex("test_virtual", test_virtual);

	if (vmm_translate(test_virtual) != 0) {
		print_step("VMM: test virtual address was unexpectedly pre-mapped");
		return 0;
	}

	print_step("VMM: allocate one physical frame for mapping");
	void* frame = pmm_alloc_frame();
	print_value_hex("frame", (uint64_t)(uintptr_t)frame);
	if (!frame) {
		print_step("VMM: failed to allocate frame for mapping");
		return 0;
	}

	print_step("VMM: map frame into current address space");
	if (vmm_map((uint64_t)(uintptr_t)frame, test_virtual, VMM_FLAG_WRITE) !=
		0) {
		print_step("VMM: vmm_map returned failure");
		pmm_free_frame(frame);
		return 0;
	}

	print_step("VMM: verify translation returns the mapped frame");
	uint64_t translated = vmm_translate(test_virtual);
	print_value_hex("translated", translated);
	if (translated != (uint64_t)(uintptr_t)frame) {
		print_step("VMM: translation mismatch after map");
		vmm_unmap(test_virtual);
		pmm_free_frame(frame);
		return 0;
	}

	print_step("VMM: perform read/write through mapped virtual address");
	volatile uint64_t* mapped_ptr = (volatile uint64_t*)(uintptr_t)test_virtual;
	*mapped_ptr = 0x1122334455667788ULL;
	if (*mapped_ptr != 0x1122334455667788ULL) {
		print_step("VMM: readback mismatch from mapped address");
		vmm_unmap(test_virtual);
		pmm_free_frame(frame);
		return 0;
	}

	print_step("VMM: unmap virtual address");
	if (vmm_unmap(test_virtual) != 0) {
		print_step("VMM: vmm_unmap returned failure");
		pmm_free_frame(frame);
		return 0;
	}

	print_step("VMM: verify translation is cleared after unmap");
	if (vmm_translate(test_virtual) != 0) {
		print_step("VMM: translation still present after unmap");
		pmm_free_frame(frame);
		return 0;
	}

	print_step("VMM: release test frame back to PMM");
	pmm_free_frame(frame);
	return 1;
}

static int run_vmm_address_space(void) {
	// 0x52000000 = 1.28 GiB + one page - distinct from run_vmm_map_unmap's
	// address so the two tests do not interfere if run in sequence.
	const uint64_t test_virtual = 0x52000000ULL;
	print_step("VMM-AS: create a new process-style address space");

	uint64_t* as = vmm_create_address_space();
	print_value_hex("address_space_pml4", (uint64_t)(uintptr_t)as);
	if (!as) {
		print_step("VMM-AS: address space creation failed");
		return 0;
	}

	print_step("VMM-AS: allocate frame for mapping in new address space");
	void* frame = pmm_alloc_frame();
	print_value_hex("frame", (uint64_t)(uintptr_t)frame);
	if (!frame) {
		print_step("VMM-AS: failed to allocate frame");
		vmm_destroy_address_space(as);
		return 0;
	}

	print_step("VMM-AS: map frame in new address space with user+write flags");
	if (vmm_map_in_space(as, (uint64_t)(uintptr_t)frame, test_virtual, VMM_FLAG_WRITE | VMM_FLAG_USER) != 0) {
		print_step("VMM-AS: vmm_map_in_space returned failure");
		pmm_free_frame(frame);
		vmm_destroy_address_space(as);
		return 0;
	}

	print_step("VMM-AS: verify translation in new address space");
	if (vmm_translate_in_space(as, test_virtual) !=
		(uint64_t)(uintptr_t)frame) {
		print_step("VMM-AS: translation mismatch after map");
		vmm_unmap_in_space(as, test_virtual);
		pmm_free_frame(frame);
		vmm_destroy_address_space(as);
		return 0;
	}

	print_step("VMM-AS: unmap from new address space");
	if (vmm_unmap_in_space(as, test_virtual) != 0) {
		print_step("VMM-AS: vmm_unmap_in_space returned failure");
		pmm_free_frame(frame);
		vmm_destroy_address_space(as);
		return 0;
	}

	print_step("VMM-AS: verify translation cleared in new address space");
	if (vmm_translate_in_space(as, test_virtual) != 0) {
		print_step("VMM-AS: translation still present after unmap");
		pmm_free_frame(frame);
		vmm_destroy_address_space(as);
		return 0;
	}

	print_step("VMM-AS: cleanup frame and destroy address space");
	pmm_free_frame(frame);
	vmm_destroy_address_space(as);
	return 1;
}

void test_pmm_basic(void) {
	print_str("\n[TEST] PMM basic\n");
	print_result("PMM basic", run_pmm_basic());
}

void test_heap_basic(void) {
	print_str("\n[TEST] Heap basic\n");
	print_result("Heap basic", run_heap_basic());
}

void test_heap_calloc_realloc(void) {
	print_str("\n[TEST] Heap calloc/realloc\n");
	print_result("Heap calloc/realloc", run_heap_calloc_realloc());
}

void test_vmm_map_unmap(void) {
	print_str("\n[TEST] VMM map/unmap\n");
	print_result("VMM map/unmap", run_vmm_map_unmap());
}

void test_vmm_address_space(void) {
	print_str("\n[TEST] VMM address space\n");
	print_result("VMM address space", run_vmm_address_space());
}

void test_memory_all(void) {
	g_total_tests = 0;
	g_passed_tests = 0;

	print_str("Running memory tests (verbose mode)...\n");
	test_pmm_basic();
	test_heap_basic();
	test_heap_calloc_realloc();
	test_vmm_map_unmap();
	test_vmm_address_space();

	print_str("\nMemory test summary:\n");
	print_value_dec("total", (uint64_t)g_total_tests);
	print_value_dec("passed", (uint64_t)g_passed_tests);
	print_value_dec("failed", (uint64_t)(g_total_tests - g_passed_tests));
}
