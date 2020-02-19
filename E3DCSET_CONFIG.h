#ifndef E3DCSET_CONFIG_h
#define E3DCSET_CONFIG_h
#endif

typedef struct {
	uint32_t MAX_LEISTUNG;
	uint32_t MIN_LADUNGSMENGE;
	uint32_t MAX_LADUNGSMENGE;
    char 	 server_ip[20];
    uint32_t server_port;
    char 	 e3dc_user[128];
    char 	 e3dc_password[128];
    char 	 aes_password[128];
    bool	 debug;
} e3dc_config_t;
