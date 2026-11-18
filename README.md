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

### Tahap 7 — Sistem Berkas (VFS RAMDisk)

Berkas teks disimpan di RAM (hilang saat reboot). Maksimal 32 berkas, nama ≤ 23 karakter, isi ≤ 4096 byte. `<n>` = nama berkas.

| Perintah | Deskripsi |
| :--- | :--- |
| `ls` | Daftar berkas: izin, pemilik, ukuran, nama. |
| `touch <n>` | Membuat berkas kosong. |
| `write <n> <teks>` | Menulis/menimpa isi berkas (`\n` = baris baru). |
| `append <n> <teks>` | Menambah teks di akhir berkas tanpa menghapus isi lama. |
| `cat <n>` | Menampilkan isi berkas. |
| `rm <n>` | Menghapus berkas. |
| `stat <n>` | Menampilkan ukuran, pemilik, dan izin satu berkas. |
| `chmod <ooo> <n>` | Mengubah izin berkas (3 digit oktal, mis. `644`). |
| `chown <user> <n>` | Mengganti pemilik berkas (hanya root). |
| `df` | Pemakaian RAMDisk (jumlah berkas, byte) dan heap. |
| `fstest` | Uji otomatis VFS: CRUD, batas ukuran, isi lama aman saat gagal tulis, tanpa kebocoran heap. |

### Tahap 8 — Proteksi (Hak Akses Sederhana)

Akun tetap di `kernel/security.c`: `root` (uid 0), `budi` (1000), `ani` (1001). Password disimpan sebagai hash djb2. OS boot sebagai root.

| Perintah | Deskripsi |
| :--- | :--- |
| `whoami` | Menampilkan user yang sedang aktif. |
| `su [user]` | Pindah user. Root pindah tanpa password; user biasa wajib password (tampil `*`). |
| `users` | Daftar akun; user aktif ditandai `<- aktif`. |
| `sectest` | Uji otomatis aturan izin, `chmod`/`chown`, dan login benar/salah. |

Aturan izin: mode oktal ala UNIX (digit 1 = pemilik, digit 3 = lainnya; 4 = baca, 2 = tulis). Root melewati semua pemeriksaan. Hapus/`chmod` hanya oleh pemilik atau root; `chown`, `reboot`, `halt` hanya root. Detail desain dan urutan demo ada di `docs/Tahap7-8.md`.
