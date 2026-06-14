#!/bin/bash
echo "========================================================"
echo "         Kayra ROV Yer Kontrol Istasyonu (GCS)"
echo "========================================================"
echo "Cihaz baslatiliyor..."

# Eğer surface-control derlenmemişse önce derle
if [ ! -f "surface-control" ]; then
    echo "Uygulama derleniyor, lutfen bekleyin..."
    make
fi

# GUI modu ile baslat
./surface-control --gui

echo "Arayuz kapatildi."
