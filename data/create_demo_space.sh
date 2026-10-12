export SERVER_CONTAINER=open-etp-server_open_etp_server_runtime_run_5981f91fc84f
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar --new -s demo/Volve
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --import-epc ./data/Volve_Demo_Faults_Depth.epc
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --import-epc ./data/Volve_Demo_Horizons_Depth.epc
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --import-epc ./data/Volve_Demo_Reservoir_Grid_Depth.epc
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --import-epc ./data/Volve_Demo_Wells_Depth.epc
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --stats
openETPServer space -S ws://$SERVER_CONTAINER:9002 -u foo -p bar -s demo/Volve --delete
