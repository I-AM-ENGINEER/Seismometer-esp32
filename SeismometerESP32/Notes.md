Получение таймингов:
1) Обычная FreeRTOS таска с максимальным приоритетом имеет разброс +- 1 мкс
2) Таймер gptimer не имеет смысла, раброс получается больше чем в 1 варианте
3) Программные таймеры работают хуже варианта 1.
Вывод: ESP не могет в жесткие тайминги, таска с высоким приоритетом самый простой и лучший способ

АЦП:
1) Сжатие не требуется, оно не оказывает значительного влияния на трафик
2) В теории достаточно 16 KSPS, 16 бит (256кбит/с)


esp_wifi_get_tsf_time(ESP_IF_WIFI_STA); - работает хуже, чем
esp_mesh_get_tsf_time();