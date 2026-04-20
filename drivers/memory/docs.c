/*
	Overall working of memory in any system:

	MMU (Memory Management Unit) is responsible for translating virtual addresses to physical addresses.
	It uses page tables to manage this translation.
	The MMU also handles memory protection, ensuring that processes cannot access memory that they are not authorized to.

	PAS (Physical Address Space) is the actual physical memory available in the system. It is divided into frames (usually 4KB each).
	The PMM (Physical Memory Manager) manages the allocation and deallocation of these physical frames.
	It keeps track of which frames are free and which are allocated, often using a bitmap for efficient tracking.

	VAS (Virtual Address Space) is the address space that each process sees. It is divided into pages (also usually 4KB each).
	The VMM (Virtual Memory Manager) manages the mapping of virtual pages to physical frames.
	It handles page faults, which occur when a process tries to access a virtual address that is not currently mapped to a physical frame.
	It may also implement features like demand paging and swapping.

	Why we need both PMM and VMM?
		- PMM is responsible for managing the actual physical memory, while VMM manages the virtual address space of processes.
		- PMM ensures that physical memory is allocated and freed correctly,
		  while VMM ensures that processes can access memory in a way that is isolated and protected from each other.
		- PMM provides the underlying physical memory resources, while VMM provides the abstraction of virtual memory that allows
		  for features like memory protection, process isolation, and efficient memory usage through techniques like paging and swapping.

	Modern Operating systems 2 different types of memory management:
		- Paging: Memory is divided into fixed-size pages (e.g., 4KB). The VMM maps virtual pages to physical frames.
		  This allows for efficient memory usage and protection.
		- Segmentation: Memory is divided into variable-sized segments based on logical divisions (e.g., code, data, stack).
		  The VMM manages these segments and their access permissions.

	For our kernel, we will implement both paging and segmentation to provide a robust memory management system that can support multiple processes.

	Logical Address: this is the physical address as seen by the CPU, which is translated by the MMU to a linear address.
		|  Segmentation unit
		│  (long mode: base=0, no-op for CS/DS/ES/SS)
		▼
	Linear Address: this equals virtual address in long mode
		│  Paging unit
		│  (PML4 walk, enforces R/W/U/NX)
		▼
	Physical Address
*/