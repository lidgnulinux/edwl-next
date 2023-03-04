# EDWL (Versi Lanjut / Next).

## Apa Ini ?

Edwl merupakan fork dari kompositor wayland [dwl](https://github.com/djpohly/dwl) yang di dalamnya ditambahkan beberapa fitur (bisa dilihat pada bagian ./README-ORIG.md). Saya menge-fork [edwl](https://gitlab.com/necrosis/edwl) dan menambah dan mengganti beberapa bagian, sejauh ini ada beberapa tambalan / patch yang saya aplikasikan.

## Penerapan Patch.

Beberapa patch yang saya aplikasikan :

- **Layout bawaan dwl**, saya kurang cocok dengan layout bawaan edwl.
- **Fix Fokus**, ketika bagian bar di klik maka jendela yang tidak memiliki fokus akan diberikan fokus dan tidak langsung ter-hide, jendela yang ter-hide akan muncul dan mendapatkan fokus.
- **Margin Bar**, menambahkan margin pada bar.
- **Fungsi Hide**, menambahkan fungsi hide untuk menyembunyikan jendela lewat pintasan / *shortcut*.
- **Fungsi Center Window**, menambahkan fungsi untuk memindah jendela tepat ke tengah monitor.
- **Fungsi Move Floating Window**, menambahkan fungsi untuk memindah jendela *floating* ke kanan, kiri, atas dan bawah.
- **Fungsi Resize Floating Window**, menambahkan fungsi untuk mengubah ukuran jendela *floating*.
- **Double Bar, (fitur experimental)**, menambahkan bar di bawah.

## Dependensi.

- Wlroots 0.15.X

## Instalasi

- Edit config.h dan config.mk, sesuaikan dengan kebutuhan.
- Jalankan perintah `make`, `sudo make install`.

## Credit.

Terima kasih yang sangat besar untuk beberapa pihak yang membantu dan memberi inspirasi :

- Tim pengembang [dwl](https://github.com/djpohly/dwl).
- Necrosis, pengembang [edwl](https://gitlab.com/necrosis/edwl).
- LIDG.
