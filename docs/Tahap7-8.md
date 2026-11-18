# Tahap 7 (VFS RAMDisk) & Tahap 8 (Proteksi) - Dalfon OS

## File baru
- include/vfs.h, kernel/vfs.c        : sistem berkas di RAM
- include/security.h, kernel/security.c : akun user, login, hash password
- include/fscmd.h, kernel/fscmd.c    : perintah shell (ls, touch, write, ... su, sectest)
- tests/                             : `make test-vfs` (uji tanpa QEMU, 105 pemeriksaan)
Diubah sedikit: kernel/kernel.c (init), kernel/shell.c (hook perintah), Makefile.

## Desain VFS
- Tabel tetap 32 entri; nama maks 23 karakter [A-Za-z0-9._-]; isi maks 4096 byte.
- Isi berkas di heap (kmalloc dari Tahap 6). Write: alokasikan buffer baru DULU, baru kfree yang lama
  -> kalau memori habis, isi lama tetap utuh (ENOMEM).
- Tidak ada folder (flat). Data hilang saat reboot (RAM).
- Error: ENOENT, EEXIST, EACCES, ENOSPC, ENOMEM, EINVAL, EFBIG.

## Desain Proteksi
- User: root(0, root123), budi(1000, budi123), ani(1001, ani123). Boot sebagai root.
- Password disimpan sebagai hash djb2 (bukan teks). Catatan: djb2 lemah, hanya untuk demo.
- Mode oktal ala UNIX (mis. 644): digit 1 = pemilik, digit 3 = lainnya; digit group disimpan tapi tidak dipakai.
- Aturan: root lolos semua cek. Hapus/chmod = pemilik atau root. chown = root saja.
- su: root pindah tanpa password; user biasa wajib password (diketik tampil '*').
- reboot, halt: hanya root.

## Urutan demo
ls -> touch a.txt -> write a.txt halo\ndunia -> append a.txt !! -> cat a.txt -> stat a.txt -> rm a.txt
-> fstest -> df -> meminfo
-> su budi -> cat rahasia.txt (ditolak) -> write readme.txt x (ditolak) -> reboot (ditolak)
-> su root (root123) -> chmod 644 rahasia.txt -> su budi -> cat rahasia.txt (berhasil) -> su root -> sectest

## Tanya-jawab dosen
- Kenapa write alokasi dulu baru free? Agar gagal OOM tidak menghilangkan data lama.
- Kenapa root mengabaikan izin? Meniru UNIX: uid 0 superuser.
- Apa kelemahan? Tanpa lock (hanya thread shell yang akses VFS), flat, RAM volatile, djb2 lemah, belum ada mode user/kernel sungguhan.
- Hubungan dengan Tahap 6? Semua isi berkas memakai kmalloc/kfree; fstest mengecek tidak ada kebocoran heap.
