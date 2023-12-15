// Definition des pins
#define interup 13 // pin interuption I2C
#define send_off 34
#define on_synchro 35
#define bas 32
#define droite 33
#define haut 25
#define gauche 26
#define a_codeuse 27
#define b_codeuse 14
#define bat 12

// definition des variable

/* gestion bouton off */
#define time_push_off 3000  // temps d'appui pour mode off en millisecondes
int last_send_off = 0;      // etat precedent
uint32_t time_off_push = 0; // enregistrement de millis()

/* gestion auto-off */
uint32_t time_start = 0;    // enregistrement de millis pour l'auto_off
uint32_t time_init_ble = 0; // enregistrement de millis pour timer si pas de connexion dans les 10 minutes

/* gestion de la synchro candle */
bool synchro = false; // verification de la synchro avec candle

/* gestion de l'I2C*/
PCF8574 *nb_mod[6];                                       // nombre de PCF 8574 sur le bus I2C
const int addr[6] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25}; // Déclaration des adresses
int last_etat_mod[6][8];                                  // enregistrement de l'état précedent des etat I2C
bool new_etat_i2c = false;                                // verification d'un changement sur I2C
// Definition des I/O des PCF8574, 0 = OUTPUT, 1 = INPUT
const int conf_mod[6][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // Module PAS
    {0, 0, 0, 0, 0, 0, 0, 1}, // Module FEED
    {1, 0, 0, 0, 1, 1, 1, 0}, // Module axe et BP commande
    {1, 1, 1, 1, 1, 1, 0, 0}, // Module 1 BP commande et scene
    {1, 1, 1, 1, 1, 1, 0, 0}, // Module 2 BP commande et scene
    {0, 0, 0, 0, 0, 0, 0, 0}  // Module led scene
};

/* gestion des pas */
#define nb_pas 7                                     // Nombre de led de PAS
const int ordre_pas[nb_pas] = {0, 1, 2, 3, 4, 5, 6}; // continue, 0.01, 0.1, 1, 5, 10, 100
int etat_pas = 0;                                    // état affichage pas
int etat_pas_last = 0;                               // etat précédent d'affichage de pas

/* gestion du feed*/
#define nb_feed 6                                   // Nombre de led de FEED
const int ordre_feed[nb_feed] = {6, 5, 4, 3, 2, 1}; // 2000, 1000, 500, 100, 50, 10
int etat_feed = 0;
int etat_feed_last = 0;

/* gestion des axes */
#define nb_axe 3                         // Nombre de led de AXE
const int ordre_axe[nb_axe] = {3, 1, 2}; // Z, X, Y
int etat_axe = 0;
int etat_axe_last = 0;

/* gestion des direction*/
int state_haut = 0;
int last_haut = 0;
int state_bas = 0;
int last_bas = 0;
int state_droite = 0;
int last_droite = 0;
int state_gauche = 0;
int last_gauche = 0;

/* gestion roue codeuse */
volatile int codeuse_down = 0;
volatile int codeuse_up = 0;
int mem_codeuse_down = 0;
int mem_codeuse_up = 0;
const uint8_t ble_axe_cdm_up[3] = {0xD3, 0xD7, 0xDA};   // commande ble touche up
const uint8_t ble_axe_cdm_down[3] = {0xD6, 0xD8, 0xD9}; // commande ble touche down

/* gestion de la batterie */
uint8_t level_batt = 0;
int val_bat;
float tension_batt = 0.0;
uint32_t time_send_bat;

// Déclaration de l'environnement BleKeyboard
BleKeyboard MyBLE(name_ble); // Initialisation de la class BleKeyboard

// fonction d'interuption I2C
void IRAM_ATTR fonction_ISR()
{
    if (synchro)
    {
        new_etat_i2c = true;
    }
}
/*************************************************************/

// fonction d'interuption roue codeuse
void IRAM_ATTR sens_codeuse()
{
    if (synchro)
    {
        if (digitalRead(a_codeuse))
        {
            if (digitalRead(b_codeuse))
            {
                codeuse_down++;
            }
            else
            {
                codeuse_up++;
            }
        }
    }
}
/*************************************************************/

// fonction d'initialisation des I/O I2C
void init_i2c_pin()
{
    int state_init = 0;
    for (int i = 0; i < 6; i++)
    {
        nb_mod[i] = new PCF8574(addr[i]); // declaration des modules I2C
    }
    // definition des I/O
    Serial.println(F("Initialisation des I/O des PCF8574 du bus I2C :"));
    for (int i = 0; i < 6; i++)
    {
        Serial.print(F("\nModule : "));
        Serial.println(i + 1);
        for (int j = 0; j < 8; j++)
        {
            last_etat_mod[i][j] = 1; // on initialise le tableau last_etat_mod
            Serial.print(F("\t port : "));
            Serial.print(j);
            switch (conf_mod[i][j])
            {
            case 1:
                pinMode(*nb_mod[i], j, INPUT_PULLUP);
                Serial.print(F(" -> INPUT - ETAT -> "));
                break;

            case 0:
                pinMode(*nb_mod[i], j, OUTPUT);
                digitalWrite(*nb_mod[i], j, 0);
                delay(50);
                digitalWrite(*nb_mod[i], j, 1);

                Serial.print(F(" -> OUTPUT - ETAT -> "));
                break;

            default:
                break;
            }
            state_init = digitalRead(*nb_mod[i], j);
            Serial.println(state_init);
        }
    }
}
/*************************************************************/

// fonction attente connexion ble
void connect_ble()
{
    time_init_ble = millis();
    // on eteint toutes les led
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (conf_mod[i][j] == 0) // si configuré en OUTPUT
            {
                digitalWrite(*nb_mod[i], j, 1);
            }
        }
    }
    Serial.print(F("\nConnexion du Bluetooth Low Energy "));
    while (!MyBLE.isConnected())
    {
        if (millis() - time_init_ble >= 300 * 1000) // 5 minutes
        {
            // on eteint toutes les led
            for (int i = 0; i < 6; i++)
            {
                for (int j = 0; j < 8; j++)
                {
                    if (conf_mod[i][j] == 0) // si configuré en OUTPUT
                    {
                        digitalWrite(*nb_mod[i], j, 1);
                    }
                }
            }
            Serial.println(F("Arret de CANDLE-BLE -> AUTO-OFF PERTE BLE"));
            esp_deep_sleep_start();
        }
        Serial.print(F(". "));
        // on fait clignoté la led axe Z en attendan la connexion BLE
        digitalWrite(*nb_mod[2], 3, 0);
        delay(250);
        digitalWrite(*nb_mod[2], 3, 1);
        delay(250);
    }
    Serial.println(F("-> [OK]\n"));
    synchro = false;
}
/*************************************************************/

// fonction niveau bat
void niv_bat()
{
    val_bat = analogRead(bat);
    tension_batt = (3.3 * val_bat / (4095 / 2));
    level_batt = constrain(map(tension_batt * 100, 270, 400, 0, 100), 0, 100); // on map et contraint la valeur pour l'envoi
    MyBLE.setBatteryLevel(level_batt);

    Serial.print(F("tension batt : "));
    Serial.print(tension_batt);
    Serial.print(F(" V - % : "));
    Serial.println(level_batt);
}
/*************************************************************/

// fonction envoi niveau bat
void send_niv_bat()
{
    if (millis() - time_send_bat >= 600 * 1000) // si plus grand que 10 minutes
    {
        time_send_bat = millis();
        if (MyBLE.isConnected())
        {

            niv_bat();
        }
        else
        {
            connect_ble();
        }
    }
}
/*************************************************************/

// fonction d'initialisation du Bluetooth Low Energy
void init_ble()
{

    Serial.print(F("\nDémarrage du Bluetooth Low Energy "));
    MyBLE.begin();
    Serial.println(F("-> [OK]\n"));

    connect_ble();
    niv_bat();
}
/*************************************************************/

// *** début des fonctions de commande ***

// fonction timer auto_off
void auto_off()
{
    if (millis() - time_start >= time_auto_off * 1000)
    {
        // on eteint toutes les led
        for (int i = 0; i < 6; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (conf_mod[i][j] == 0) // si configuré en OUTPUT
                {
                    digitalWrite(*nb_mod[i], j, 1);
                }
            }
        }
        Serial.println(F("Arret de CANDLE-BLE -> AUTO-OFF"));
        esp_deep_sleep_start();
    }
}
/*************************************************************/

// fonction BP off
void off()
{
    int state_off = 0;
    state_off = digitalRead(send_off);
    if (last_send_off == 0 && state_off == 1) // on bloque la tempo
    {
        time_off_push = millis();
        last_send_off = 1;
    }
    if (last_send_off == 1 && state_off == 1 && (millis() - time_off_push) >= time_push_off) // si tempo + temps on arrete le module
    {
        // on eteint toutes les led
        for (int i = 0; i < 6; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (conf_mod[i][j] == 0) // si configuré en OUTPUT
                {
                    digitalWrite(*nb_mod[i], j, 1);
                }
            }
        }
        Serial.println(F("Arret de CANDLE-BLE -> BOUTON OFF"));
        esp_deep_sleep_start(); // on passe en mode sommeil
    }
    if (last_send_off == 1 && state_off == 0 && (millis() - time_off_push) <= 1000) // si appuyé moins d'une seconde on envoi le programme
    {
        last_send_off = 0;
        MyBLE.press(KEY_LEFT_CTRL);
        MyBLE.press(KEY_NUM_ASTERISK);
        Serial.println(F("Envoie du programme d'usinage"));
        delay(50);
        MyBLE.releaseAll();
    }
    if (last_send_off == 1 && state_off == 0)
    { // si relachement du bouton on stop le blocage de la tempo
        last_send_off = 0;
    }
}
/*************************************************************/

// fonction de commande de pas
void cdm_pas()
{
    if (synchro)
    {

        if (etat_pas >= nb_pas) // on revient a zero si arrivé au max led
        {
            etat_pas = 0;
        }
        // si etat pas = 0 passage en mode continue
        if (etat_pas <= 0)
        {
            for (int p = 0; p < nb_pas; p++) // passage en mode continue
            {
                MyBLE.press(KEY_F9);
                delay(10);
                MyBLE.releaseAll();
            }
        }
        // sinon on incrémente les pas
        else
        {
            MyBLE.press(KEY_F10);
            delay(50);
            MyBLE.releaseAll();
        }

        digitalWrite(*nb_mod[0], ordre_pas[etat_pas_last], 1); // on eteint la led precedente
        digitalWrite(*nb_mod[0], ordre_pas[etat_pas], 0);      // on allume la bonne valeur
        etat_pas_last = etat_pas;
    }
}
/*************************************************************/

// fonction de commande de feed
void cdm_feed()
{
    if (synchro)
    {

        if (etat_feed >= nb_feed) // on revient a zero si arrivé au max led
        {
            etat_feed = 0;
        }
        // si etat pas = 0 passage en mode continue
        if (etat_feed <= 0)
        {
            for (int p = 0; p < nb_feed; p++) // passge en mode continue
            {
                MyBLE.press(KEY_F12);
                delay(10);
                MyBLE.releaseAll();
            }
        }
        // sinon on incrémente les pas
        else
        {
            MyBLE.press(KEY_F11);
            delay(10);
            MyBLE.releaseAll();
        }

        digitalWrite(*nb_mod[1], ordre_feed[etat_feed_last], 1); // on eteint la led precedente
        digitalWrite(*nb_mod[1], ordre_feed[etat_feed], 0);      // on allume la bonne valeur
        etat_feed_last = etat_feed;
    }
}
/*************************************************************/

// fonction de commande de feed
void cdm_axe()
{
    if (synchro)
    {

        if (etat_axe >= nb_axe) // on revient a zero si arrivé au max led
        {
            etat_axe = 0;
        }

        digitalWrite(*nb_mod[2], ordre_axe[etat_axe_last], 1); // on eteint la led precedente
        digitalWrite(*nb_mod[2], ordre_axe[etat_axe], 0);      // on allume la bonne valeur
        etat_axe_last = etat_axe;
    }
}
/*************************************************************/

// fonction de commande led scene
void led_scene(int scene)
{
    for (int i = 0; i <= 6; i++)
    {
        if (i + 1 == scene)
        {

            digitalWrite(*nb_mod[5], i, 0);
        }
        else
        {
            digitalWrite(*nb_mod[5], i, 1);
        }
    }
}
/*************************************************************/

// fonction d'action sur changement d'état
void cdm_i2c()
{
    int state = 0;
    if (new_etat_i2c)
    {
        if (synchro)
        {
            new_etat_i2c = false;
            for (int i = 0; i < 6; i++)
            {
                for (int j = 0; j < 8; j++)
                {
                    state = digitalRead(*nb_mod[i], j);
                    if (conf_mod[i][j] == 1)
                    { // on verifie que se soit un input

                        if (state == 0 && last_etat_mod[i][j] != 0) // on cherche le bouton appuyez
                        {
                            Serial.print(F("changement d'état module PCF8574 -> "));
                            Serial.print(i + 1);
                            Serial.print(F(" - @ 0x"));
                            Serial.print(addr[i], HEX);
                            Serial.print(F(" - port : "));
                            Serial.println(j);
                            time_start = millis();

                            if (i == 0 && j == 7) // bouton pas
                            {
                                etat_pas++;
                                cdm_pas();
                            }
                            if (i == 1 && j == 7) // bouton feed
                            {
                                etat_feed++;
                                cdm_feed();
                            }
                            if (i == 2 && j == 0) // bouton axe
                            {
                                etat_axe++;
                                cdm_axe();
                            }
                            if (i == 2 && j == 4) // bouton homing
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_NUM_1);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 2 && j == 5) // bouton init zero Z
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F2);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 2 && j == 6) // bouton init zero XY
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F1);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 2) // bouton zero travail
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F4);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 3) // bouton zero Z
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F5);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 2) // bouton probe 1
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F9);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 3) // bouton probe 2
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F10);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 4) // bouton probe 3
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F11);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 5) // bouton probe 4
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_F12);
                                delay(50);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 1) // bouton scene 1
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F1);
                                led_scene(1);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                            if (i == 3 && j == 0) // bouton scene 2
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F2);
                                led_scene(2);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 1) // bouton scene 3
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F3);
                                led_scene(3);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 0) // bouton scene 4
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F4);
                                led_scene(4);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 5) // bouton scene 5
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F5);
                                led_scene(5);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                            if (i == 4 && j == 4) // bouton scene 6
                            {
                                MyBLE.press(KEY_LEFT_CTRL);
                                MyBLE.press(KEY_LEFT_SHIFT);
                                MyBLE.press(KEY_F6);
                                led_scene(6);
                                delay(100);
                                MyBLE.releaseAll();
                            }
                        }
                    }
                    last_etat_mod[i][j] = state;
                }
            }
        }
    }
}

/*************************************************************/

// fonction de synchronisation

void cdm_synchro()
{
    if (!synchro)
    {
        int state_synchro = 0;
        Serial.print(F("\nAttente de la synchronisation avec CANDLE "));
        while (!synchro && MyBLE.isConnected())
        {
            if (millis() - time_init_ble >= 300 * 1000) // si pas de synchro dans les 5 minutes
            {
                // on eteint toutes les led
                for (int i = 0; i < 6; i++)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        if (conf_mod[i][j] == 0) // si configuré en OUTPUT
                        {
                            digitalWrite(*nb_mod[i], j, 1);
                        }
                    }
                }
                Serial.println(F("Arret de CANDLE-BLE -> AUTO-OFF TIME-OUT SYNCHRO"));
                esp_deep_sleep_start();
            }
            Serial.print(F(". "));
            // on fait clignoter la led axe Y
            digitalWrite(*nb_mod[2], 2, 0);
            delay(250);
            digitalWrite(*nb_mod[2], 2, 1);
            delay(250);
            state_synchro = digitalRead(on_synchro);
            if (state_synchro == 1)
            {
                synchro = true;
                Serial.println(F("-> [OK]\n"));
                etat_pas = 0; // on remet les variable d'etat a zero
                cdm_pas();
                etat_feed = 0;
                cdm_feed();
                etat_axe = 0;
                cdm_axe();
            }
        }
    }
}
/*************************************************************/

// *** fin des fonctions de commande ***

// FONCTION CORE 0
void Move(void *pvParameters)
{

    while (true)
    {

        if (MyBLE.isConnected())
        {

            if (synchro)
            {
                /*** controle joystick ***/
                state_haut = digitalRead(haut);
                state_bas = digitalRead(bas);
                state_droite = digitalRead(droite);
                state_gauche = digitalRead(gauche);

                // commande haut
                if (state_haut == 1 && last_haut == 0)
                {
                    Serial.println(F("JOYSTICK -> haut"));
                    MyBLE.press(KEY_UP_ARROW);
                    last_haut = 1;
                    delay(10);
                }
                if (state_haut == 0 && last_haut == 1)
                {
                    last_haut = 0;
                    MyBLE.releaseAll();
                    delay(10);
                    time_start = millis();
                }

                // commande bas
                if (state_bas == 1 && last_bas == 0)
                {
                    Serial.println(F("JOYSTICK -> bas"));
                    MyBLE.press(KEY_DOWN_ARROW);
                    last_bas = 1;
                    delay(10);
                }
                if (state_bas == 0 && last_bas == 1)
                {
                    last_bas = 0;
                    MyBLE.releaseAll();
                    delay(10);
                    time_start = millis();
                }

                // commande droite
                if (state_droite == 1 && last_droite == 0)
                {
                    Serial.println(F("JOYSTICK -> droite"));
                    MyBLE.press(KEY_RIGHT_ARROW);
                    last_droite = 1;
                    delay(10);
                }
                if (state_droite == 0 && last_droite == 1)
                {
                    last_droite = 0;
                    MyBLE.releaseAll();
                    delay(10);
                    time_start = millis();
                }

                // commande gauche
                if (state_gauche == 1 && last_gauche == 0)
                {
                    Serial.println(F("JOYSTICK -> gauche"));
                    MyBLE.press(KEY_LEFT_ARROW);
                    last_gauche = 1;
                    delay(10);
                }
                if (state_gauche == 0 && last_gauche == 1)
                {
                    last_gauche = 0;
                    MyBLE.releaseAll();
                    delay(10);
                    time_start = millis();
                }
                /*** fin controle joystick ***/

                /*** controle roue codeuse ***/
                if (codeuse_up >= 1) // action codeuse droite
                {
                    if (etat_pas == 0 && codeuse_up >= 1) // si on est en mode continue
                    {
                        if (mem_codeuse_up == 0)
                        {
                            MyBLE.press(ble_axe_cdm_up[etat_axe]);
                            mem_codeuse_up = 1;
                            delay(100);
                        }
                    }
                    if (etat_pas >= 1 && etat_pas <= 5 && codeuse_up >= 1) // si pas de 0.01 -> 10
                    {
                        MyBLE.press(ble_axe_cdm_up[etat_axe]);
                        delay(50);
                        MyBLE.releaseAll();
                        time_start = millis();
                    }

                    if (codeuse_up == 1)
                    {
                        MyBLE.releaseAll();
                        mem_codeuse_up = 0;
                        time_start = millis();
                    }
                    codeuse_up--;
                    Serial.print(F("droite = "));
                    Serial.println(codeuse_up);
                }
                else
                {
                    if (codeuse_down >= 1) // action codeuse gauche
                    {
                        if (etat_pas == 0 && codeuse_down >= 1) // si on est en mode continue
                        {
                            if (mem_codeuse_down == 0)
                            {
                                MyBLE.press(ble_axe_cdm_down[etat_axe]);
                                mem_codeuse_down = 1;
                                delay(100);
                            }
                        }
                        if (etat_pas >= 1 && etat_pas <= 5 && codeuse_down >= 1) // si pas de 0.01 -> 10
                        {
                            MyBLE.press(ble_axe_cdm_down[etat_axe]);
                            delay(50);
                            MyBLE.releaseAll();
                            time_start = millis();
                        }

                        if (codeuse_down == 1)
                        {
                            MyBLE.releaseAll();
                            mem_codeuse_down = 0;
                            time_start = millis();
                        }
                        codeuse_down--;
                        Serial.print(F("gauche = "));
                        Serial.println(codeuse_down);
                    }
                }
                /*** fin controle roue codeuse ***/
            }
        }
        else
        {
            connect_ble();
        }
        delay(10);
    }
}
