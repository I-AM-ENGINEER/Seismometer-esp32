Results:

Execution time of TSF function in different condition

| Test Case                 | Mean Time (us)| Std Dev (us)|
|---------------------------|---------------|-------------|
| mesh_tsf_esp32AP          | 3.455675      | 1.817601    |
| mesh_tsf_laptopAP         | 2.888725      | 1.714970    |
| mesh_tsf_phoneAP          | 3.414817      | 1.798213    |
| mesh_tsf_phoneAP_iperf    | 3.472583      | 1.817823    |
| mesh_tsf_routerAP         | 4.955800      | 1.043467    |
| wifi_tsf_esp32AP          | 297.645867    | 75.366655   |
| wifi_tsf_laptopAP         | 292.663058    | 65.860994   |
| wifi_tsf_phoneAP          | 273.706842    | 108.616751  |
| wifi_tsf_phoneAP_iperf    | 301.833117    | 81.167719   |
| wifi_tsf_routerAP         | 307.652442    | 90.566536   |

esp_mesh_get_tsf_time() much better than esp_wifi_get_tsf_time():
1) Less execution time
2) Less deviation

Network load doesnt affect to tsf timestamp

Accuracy of TSF functions doesnt much depand on access point device
