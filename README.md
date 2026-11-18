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
│   ├── sync.c              # Tahap 4: Sinkronisasi Mutex & Counting/Binary Semaphore (antrean FIFO)
│   ├── deadlock.c          # Tahap 5: Demo deadlock (mutex berlawanan), deteksi wait-for graph, & pencegahan lock ordering
│   ├── memory.c            # Tahap 6: Alokator memori dinamis (kmalloc/kfree, first-fit, split & coalesce, aman saat OOM)
│   ├── vfs.c               # Tahap 7: VFS RAMDisk (32 berkas, CRUD teks, isi di heap kmalloc, izin oktal)
│   ├── security.c          # Tahap 8: akun user (root/budi/ani), hash password djb2, login (su)
│   └── fscmd.c             # Tahap 7 & 8: perintah CLI berkas/pengguna + uji otomatis fstest & sectest
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
│   ├── vfs.h               # Header API VFS RAMDisk & kode error (ENOENT, EACCES, ...)
│   ├── security.h          # Header API akun user & autentikasi
│   ├── fscmd.h             # Header perintah CLI berkas/pengguna
│   └── shell.h             # Header deklarasi fungsi CLI shell & utilitas string
├── tests/
│   └── test_vfs.c          # Uji VFS & proteksi tanpa QEMU (make test-vfs)
├── docs/
│   └── Tahap7-8.md         # Catatan desain, urutan demo, tanya-jawab Tahap 7 & 8
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

## Daftar Perintah CLI Shell (`user@dalfon# `)

Saat **Dalfon OS v0.4.0** menyala, Anda akan disambut oleh logo ASCII `DALFON OS` dan prompt interaktif `root@dalfon# ` (root) atau `user@dalfon$ ` (user biasa). Berikut adalah perintah yang didukung:

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
| `deadlock` | Tahap 5 | Menjalankan pengujian **Tahap 5 (Deadlock)**: 2 thread memakai 2 mutex dengan urutan berlawanan hingga *deadlock*, mendeteksinya lewat *wait-for graph*, lalu membuktikan **lock ordering** mencegahnya. |
| `meminfo` | Tahap 6 | Menampilkan kondisi heap kernel: rentang alamat, memori **terpakai** dan **tersedia**, jumlah blok, dan statistik alokasi. |
| `memtest` | Tahap 6 | Menjalankan pengujian **Tahap 6 (Alokator Memori)**: `kmalloc`/`kfree`, perubahan memori terpakai/tersedia, *coalescing*, penanganan **Out of Memory** tanpa crash, dan penolakan `kfree` tidak valid. |
| `uptime` | Tahap 3 | Menampilkan jumlah *timer ticks* (ms) dan detik sejak sistem *booting*. |
| `reboot` | Tahap 1 | Melakukan *warm reboot* sistem melalui kontroler PS/2 (`0x64`). |
| `halt` | Tahap 1 | Menghentikan eksekusi CPU (`cli; hlt`). |
| `ls` | Tahap 7 | Menampilkan daftar berkas di **RAMDisk**: izin, pemilik, ukuran, dan nama. |
| `touch <n>` | Tahap 7 | Membuat berkas kosong bernama `<n>` (nama ≤ 23 karakter: huruf, angka, `.`, `_`, `-`). |
| `write <n> <teks>` | Tahap 7 | Menulis atau **menimpa** isi berkas (`\n` = baris baru). Isi lama tetap aman bila penulisan gagal. |
| `append <n> <teks>` | Tahap 7 | **Menambah** teks di akhir berkas tanpa menghapus isi lama. |
| `cat <n>` | Tahap 7 | Menampilkan isi berkas. |
| `rm <n>` | Tahap 7 | Menghapus berkas dan mengembalikan memorinya ke heap (`kfree`). |
| `stat <n>` | Tahap 7 | Menampilkan ukuran, pemilik, dan izin satu berkas. |
| `chmod <ooo> <n>` | Tahap 7 & 8 | Mengubah **izin** berkas dengan 3 digit oktal, mis. `644` (4 = baca, 2 = tulis; digit pertama pemilik, ketiga pengguna lain). Hanya pemilik atau root. |
| `chown <user> <n>` | Tahap 7 & 8 | Mengganti **pemilik** berkas. Hanya root. |
| `df` | Tahap 7 | Menampilkan pemakaian RAMDisk (jumlah berkas, byte data) dan heap kernel. |
| `fstest` | Tahap 7 | Menjalankan pengujian **Tahap 7 (VFS)**: CRUD, batas ukuran 4096 byte (`EFBIG`), nama tidak valid, isi lama aman saat gagal tulis, dan **tanpa kebocoran heap**. |
| `whoami` | Tahap 8 | Menampilkan **user yang sedang aktif** (nama dan uid). |
| `su [user]` | Tahap 8 | Berpindah user. Root pindah **tanpa password**; user biasa wajib password (tampil `*`). Tanpa argumen = ke `root`. |
| `users` | Tahap 8 | Menampilkan daftar akun (`root`, `budi`, `ani`); user aktif ditandai `<- aktif`. |
| `sectest` | Tahap 8 | Menjalankan pengujian **Tahap 8 (Proteksi)**: matriks izin root/budi/ani, aturan `chmod`/`chown`, dan login benar/salah. |

**Catatan Tahap 7 & 8**
- **RAMDisk:** maksimal 32 berkas, isi ≤ 4096 byte, disimpan di heap `kmalloc`; data hilang saat reboot. `<n>` = nama berkas.
- **Akun:** tetap di `kernel/security.c` — `root` (uid 0), `budi` (1000), `ani` (1001); password disimpan sebagai hash djb2 (bukan teks). OS boot sebagai root.
- **Aturan izin:** root melewati semua pemeriksaan. Hapus dan `chmod` hanya oleh pemilik atau root; `chown`, `reboot`, dan `halt` hanya root.
- **Prompt:** `root@dalfon# ` untuk root, `user@dalfon$ ` untuk user biasa.
- Detail desain dan urutan demo ada di `docs/Tahap7-8.md`.
