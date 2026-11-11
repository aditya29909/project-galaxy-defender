# Galaxy Defender

Game arcade luar angkasa menggunakan C99 dan raylib. Bertahan dari serangan alien, kumpulkan kredit, dan tingkatkan senjata untuk menghadapi tiga boss.

## Build

Persyaratan: CMake 3.16+, compiler C, dan koneksi internet saat build pertama jika raylib 5.5 belum terpasang. CMake menggunakan raylib yang sudah terpasang, atau mengunduh raylib 5.5 otomatis.

```sh
cmake -S . -B build
cmake --build build --config Release
```

Jalankan `build/galaxy_defender` di Linux/macOS, atau `build/Release/galaxy_defender.exe` di Windows dengan Visual Studio. Dengan generator MinGW, executable berada di `build/galaxy_defender.exe`.

Contoh Windows dengan MinGW:

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
./build/galaxy_defender.exe
```

## Cara bermain

- Menu: tombol panah atau W/S untuk memilih, Enter untuk konfirmasi, Esc untuk kembali.
- Permainan: A/D atau panah kiri/kanan untuk bergerak; tahan Space untuk menembak; P untuk jeda.
- Campaign terdiri dari 3 chapter, masing-masing 10 level. Level 10 adalah pertarungan boss. Lanjutkan level dengan Enter setelah menang.
- Survival terus menghasilkan gelombang yang semakin sulit sampai nyawa habis.
- Pilih satu side mission sebelum memulai permainan. Selesaikan target dalam satu permainan untuk memperoleh hadiah kredit acak yang sudah ditampilkan; misi baru tersedia setelah misi selesai.
- Kredit dari alien, boss, dan misi dapat dipakai di menu Upgrade untuk meningkatkan damage secara permanen. Kredit dan level senjata disimpan di `galaxy_defender.save` pada direktori kerja saat game ditutup atau nilai berubah.
- Pada layar hasil, Enter mengulang mode yang sama dan Esc kembali ke menu utama.

Semua tampilan dibuat dengan primitive raylib; tidak ada aset tambahan yang diperlukan.
