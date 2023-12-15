/*
███████╗███╗   ███╗██╗  ██╗ ██████╗ ███████╗██╗   ██╗
██╔════╝████╗ ████║██║  ██║██╔═══██╗██╔════╝╚██╗ ██╔╝
███████╗██╔████╔██║███████║██║   ██║███████╗ ╚████╔╝
╚════██║██║╚██╔╝██║██╔══██║██║   ██║╚════██║  ╚██╔╝
███████║██║ ╚═╝ ██║██║  ██║╚██████╔╝███████║   ██║
╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝
Projet CANDLE-BLE
crée le :     01/11/2023
par :         David AUVRÉ alias SMHOSY

*** remerciement ***
Merci aux membres du discord pour le soutien et l'ajout de certaines fonctionnalitée
*/

// Chargement des librairie
#include <pcf8574.h> // Librairie légère de gestion de PCF8574 MULTIPLE https://github.com/MSZ98/PCF8574
#include <BleKeyboard.h> // https://github.com/T-vK/ESP32-BLE-Keyboard modifié pour le mode AZERTY https://github.com/davi-domo/ESP32-BLE-Keyboard-AZERTY

// chargement du fichier de configuration
#include <config.h>

// chargement des class et fonction
#include <fonction.h>

// defifinition du mode de debug
#define DEBUG true
#define Serial \
    if (DEBUG) \
    Serial

void setup()
{
    // declaration du port serie pour le DEBUG
    Serial.begin(115200);

    // initialisation des GPIO
    pinMode(send_off, INPUT_PULLDOWN);
    pinMode(on_synchro, INPUT_PULLDOWN);
    pinMode(bas, INPUT_PULLDOWN);
    pinMode(haut, INPUT_PULLDOWN);
    pinMode(droite, INPUT_PULLDOWN);
    pinMode(gauche, INPUT_PULLDOWN);
    pinMode(a_codeuse, INPUT_PULLDOWN);
    pinMode(b_codeuse, INPUT_PULLDOWN);
    pinMode(bat, INPUT);

    // on configure les deep_sleep et timer
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1);
    time_start = millis(); //  on enristre millis ppour le timer d'extinction atomatique
    // mode debug présentation
    Serial.println(F("Projet CANDLE-BLE télécommande BLE du lociciel CANDLE pour la CNC"));
    Serial.print(F("Version : "));
    Serial.println(ver);
    Serial.print(F("Derniere modification : "));
    Serial.println(modif);
    Serial.println(F("DEBUG ACTIVE\n"));
    /*************************************************************/

    // Initialisation des interuprion I2C
    pinMode(interup, INPUT_PULLUP);
    attachInterrupt(interup, fonction_ISR, FALLING); // Interuption sur le front décendant évite les rebonds
    /*************************************************************/

    // initialisation des interuption encodeur
    attachInterrupt(a_codeuse, sens_codeuse, RISING); // Interuption sur le front montant pour vérifier le delta avec B
    /*************************************************************/

    // initialisation des pin des PCF8574
    init_i2c_pin();
    /*************************************************************/

    // Initialisation du BLE
    init_ble();
    /*************************************************************/

    // definition du multitache
    xTaskCreate(Move, "vTask1", 4096, NULL, 1, NULL);
    delay(100);
    /*************************************************************/
}

void loop()
{
    delay(10);
    auto_off();
    off();
    cdm_synchro();
    cdm_i2c();
    send_niv_bat();
}