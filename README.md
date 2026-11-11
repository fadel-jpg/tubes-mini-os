# Dalfon OS — Mini-OS (x86 32-bit Bare-Metal Kernel)

Proyek sistem operasi *bare-metal* **Dalfon OS** 32-bit (`i686`) yang dibuat dari nol menggunakan bahasa **C (freestanding)** dan **NASM Assembly** dengan standar **GNU Multiboot v1** untuk memenuhi Tugas Besar Sistem Operasi.

---

## Struktur Direktori

```text
mini-os/
├── Makefile                # Build system (build, run, iso, clean)
├── linker.ld               # Linker script untuk memuat kernel di memori fisik 1MB (0x100000)
├── setup.sh                # Script bash pengecekan & instalasi otomatis toolchain di Linux/WSL
├── boot/
│   └── boot.asm            # Header Multiboot v1, stack 16KB, _start, context_switch, & ISR/IRQ0 stubs
├── kernel/
│   ├── kernel.c            # Fungsi kernel_main(), verifikasi Multiboot, dan inisialisasi sistem
│   ├── shell.c             # Interactive CLI Shell (parser & handler perintah)
│   ├── thread.c            # Tahap 2: Pengelolaan Kernel Thread (TCB, context switch, 5 status thread)
│   ├── idt.c               # Tahap 3: GDT, IDT (Exception 0-31 + IRQ0), remap PIC 8259, & PIT 1000 Hz
│   ├── sched.c             # Tahap 3: Preemptive Round Robin Scheduler berbasis Timer Interrupt (IRQ0)
│   └── sync.c              # Tahap 4: Sinkronisasi Mutex & Counting/Binary Semaphore (antrean FIFO)
├── drivers/
│   ├── vga.c               # Driver layar VGA Text Mode (80x25 @ 0xB8000, scrolling, kursor, warna)
│   └── keyboard.c          # Driver input PS/2 Keyboard berbasis polling (Port 0x60 & 0x64)
├── include/
│   ├── vga.h               # Header deklarasi fungsi VGA & port I/O (inb/outb)
│   ├── keyboard.h          # Header deklarasi fungsi keyboard PS/2
│   ├── thread.h            # Header struktur data Thread Control Block (TCB) & status thread
│   ├── idt.h               # Header GDT/IDT & timer frekuensi PIT (1000 Hz)
│   ├── irq.h               # Header helper critical section interrupt (irq_save, irq_restore, cli, sti)
│   ├── sched.h             # Header API Preemptive Round Robin Scheduler
│   ├── sync.h              # Header struktur data waitq_t, mutex_t, dan semaphore_t
│   └── shell.h             # Header deklarasi fungsi CLI shell & utilitas string
└── README.md               # Dokumentasi proyek
```

---

## Persyaratan Toolchain (Linux / WSL)

Untuk melakukan kompilasi dan menjalankan Dalfon OS di **WSL (Windows Subsystem for Linux)** atau **Ubuntu/Debian**, pastikan paket berikut terinstal:

- `gcc` (kompilasi C target 32-bit `-m32` freestanding)
- `nasm` (assembler untuk `boot.asm`)
- `binutils` (`ld` linker)
- `make` (build automation)
- `qemu-system-x86` (`qemu-system-i386` emulator)

### Setup Otomatis
Jalankan script `setup.sh` untuk memeriksa dan menginstal seluruh dependensi secara otomatis:

```bash
cd mini-os
chmod +x setup.sh
./setup.sh
```

---

## Cara Build & Menjalankan di QEMU

### 1. Build Kernel Binary (`bin/kernel.bin`)
```bash
make build
```

### 2. Jalankan Langsung di QEMU (Direct Multiboot)
```bash
make run
# Atau jalankan manual:
qemu-system-i386 -kernel bin/kernel.bin
```

### 3. Membersihkan File Hasil Build
```bash
make clean
```

---

## Daftar Perintah CLI Shell (`dalfon> `)

Saat **Dalfon OS v0.4.0** menyala, Anda akan disambut oleh logo ASCII `DALFON OS` dan prompt interaktif `dalfon> `. Berikut adalah perintah yang didukung:

| Perintah | Tahap | Deskripsi |
| :--- | :---: | :--- |
| `help` | Tahap 1 | Menampilkan daftar seluruh perintah yang tersedia di Dalfon OS. |
| `clear` | Tahap 1 | Membersihkan layar terminal VGA dan mengembalikan kursor ke kiri atas. |
| `about` | Tahap 1 | Menampilkan informasi arsitektur dan spesifikasi Dalfon OS. |
| `banner` | Tahap 1 | Menampilkan kembali logo ASCII art Dalfon OS. |
| `echo <teks>` | Tahap 1 | Mencetak kembali teks argumen ke layar terminal. |
| `ps` | Tahap 2 | Menampilkan tabel status **Thread Control Block (TCB)** beserta `Step`, `Ticks`, dan `Stack Ptr` (`ESP`). |
| `thread` | Tahap 2 | Menjalankan pengujian **Tahap 2**: membuat 2 kernel thread berbeda (`worker-prime` & `worker-sensor`) dan mendemonstrasikan 5 status thread (`NEW`, `READY`, `RUNNING`, `WAITING`, `TERMINATED`). |
| `sched` | Tahap 3 | Menjalankan pengujian **Tahap 3 (Preemptive Round Robin Scheduler)**: 3 worker *CPU-bound* tanpa `yield()` dipaksa bergantian oleh *timer interrupt* PIT IRQ0 (quantum 50 ms). |
| `sync` | Tahap 4 | Menjalankan pengujian **Tahap 4 (Sinkronisasi)**: membandingkan hasil operasi *shared counter* 2 thread (4.000.000 increment) tanpa sinkronisasi (*race condition*) vs dengan **Mutex** & **Semaphore** (*konsisten*). |
| `uptime` | Tahap 3 | Menampilkan jumlah *timer ticks* (ms) dan detik sejak sistem *booting*. |
| `reboot` | Tahap 1 | Melakukan *warm reboot* sistem melalui kontroler PS/2 (`0x64`). |
| `halt` | Tahap 1 | Menghentikan eksekusi CPU (`cli; hlt`). |
