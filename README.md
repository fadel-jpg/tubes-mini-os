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
│   └── boot.asm            # Header Multiboot v1, alokasi stack 16KB, entry point _start & context_switch
├── kernel/
│   ├── kernel.c            # Fungsi kernel_main(), verifikasi Multiboot, dan inisialisasi sistem
│   ├── shell.c             # Interactive CLI Shell (parser & handler perintah)
│   └── thread.c            # Pengelolaan Kernel Thread (TCB, perubahan status, dan demo 2 thread)
├── drivers/
│   ├── vga.c               # Driver layar VGA Text Mode (80x25 @ 0xB8000, scrolling, kursor, warna)
│   └── keyboard.c          # Driver input PS/2 Keyboard berbasis polling (Port 0x60 & 0x64)
├── include/
│   ├── vga.h               # Header deklarasi fungsi VGA & port I/O (inb/outb)
│   ├── keyboard.h          # Header deklarasi fungsi keyboard PS/2
│   ├── thread.h            # Header struktur data Thread Control Block (TCB) & status thread
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

Saat **Dalfon OS** menyala, Anda akan disambut oleh logo ASCII `DALFON OS` dan prompt interaktif `dalfon> `. Berikut adalah perintah yang didukung:

| Perintah | Deskripsi |
| :--- | :--- |
| `help` | Menampilkan daftar seluruh perintah yang tersedia di Dalfon OS. |
| `clear` | Membersihkan layar terminal VGA dan mengembalikan kursor ke kiri atas. |
| `about` | Menampilkan informasi arsitektur dan spesifikasi Dalfon OS. |
| `banner` | Menampilkan kembali logo ASCII art Dalfon OS. |
| `echo <teks>` | Mencetak kembali teks argumen ke layar terminal. |
| `ps` | Menampilkan tabel status **Thread Control Block (TCB)** beserta alamat *Stack Pointer* (`ESP`). |
| `thread` | Menjalankan pengujian **Tahap 2**: membuat 2 kernel thread berbeda (`worker-prime` & `worker-sensor`) dan mendemonstrasikan perubahan status (`NEW` -> `READY` -> `RUNNING` -> `WAITING` -> `TERMINATED`) via *context switch* nyata. |
| `reboot` | Melakukan *warm reboot* sistem melalui kontroler PS/2 (`0x64`). |
| `halt` | Menghentikan eksekusi CPU (`cli; hlt`). |
