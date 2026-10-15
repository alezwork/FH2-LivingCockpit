/* labels.h -- the switches' names and descriptions in the HUD settings, in the game's languages (strtab.h's order):
   InGame.IDS_HudOption_LivingCamera(_Desc), InGame.IDS_HudOption_MotionBlur(_Desc). cz, HU, DEV and LOC: English, as
   the game's own text there. UTF-8. */
#pragma once

static const char *const kLabelKey[4] = {"IDS_HudOption_LivingCamera", "IDS_HudOption_LivingCamera_Desc",
                                         "IDS_HudOption_MotionBlur", "IDS_HudOption_MotionBlur_Desc"};

#define LABELS_EN                                                                                                      \
    {"Living Camera", "In the cockpit view the driver's head reacts to the car: turns, braking, slides and speed.",  \
     "Motion Blur", "Turn the game's motion blur on or off in every view."}

static const char *const kLabel[][4] = {
    /* EN */ LABELS_EN,
    /* GB */ LABELS_EN,
    /* DE */
    {"Lebendige Kamera", "In der Cockpitansicht reagiert der Kopf des Fahrers auf das Auto: Kurven, Bremsen, Drifts und Tempo.",
     "Bewegungsunschärfe", "Bewegungsunschärfe in allen Ansichten ein-/ausschalten."},
    /* FR */
    {"Caméra vivante", "En vue cockpit, la tête du pilote réagit à la voiture : virages, freinages, dérapages et vitesse.",
     "Flou de mouvement", "Activez/désactivez le flou de mouvement dans toutes les vues."},
    /* IT */
    {"Visuale dinamica", "Nella visuale abitacolo la testa del pilota reagisce all'auto: curve, frenate, derapate e velocità.",
     "Sfocatura di movimento", "Attiva/disattiva la sfocatura di movimento in tutte le visuali."},
    /* ES */
    {"Cámara dinámica", "En la vista de cabina, la cabeza del piloto reacciona al coche: curvas, frenadas, derrapes y velocidad.",
     "Desenfoque de movimiento", "Activa o desactiva el desenfoque de movimiento en todas las vistas."},
    /* MX */
    {"Cámara dinámica", "En la vista de cabina, la cabeza del piloto reacciona al auto: curvas, frenadas, derrapes y velocidad.",
     "Desenfoque de movimiento", "Activa o desactiva el desenfoque de movimiento en todas las vistas."},
    /* br */
    {"Câmera dinâmica", "Na visão do cockpit, a cabeça do piloto reage ao carro: curvas, frenagens, derrapagens e velocidade.",
     "Desfoque de movimento", "Ligue ou desligue o desfoque de movimento em todas as visões."},
    /* PL */
    {"Dynamiczna kamera", "W widoku z kokpitu głowa kierowcy reaguje na samochód: zakręty, hamowanie, poślizgi i prędkość.",
     "Rozmycie ruchu", "Włącz lub wyłącz rozmycie ruchu we wszystkich widokach."},
    /* nl */
    {"Dynamische camera", "In het cockpitzicht reageert het hoofd van de bestuurder op de auto: bochten, remmen, slippen en snelheid.",
     "Bewegingsonscherpte", "Zet de bewegingsonscherpte in alle weergaven aan/uit."},
    /* RU */
    {"Живая камера", "В виде из кабины голова водителя реагирует на машину: повороты, торможение, заносы и скорость.",
     "Размытие в движении", "Включение или выключение размытия в движении во всех видах."},
    /* JP */
    {"ダイナミック カメラ", "コックピット ビューでドライバーの頭がクルマの動きに反応します (コーナー、ブレーキ、スライド、スピード)",
     "モーション ブラー", "すべてのビューでモーション ブラーの On/Off を選択します"},
    /* KO */
    {"다이내믹 카메라", "콕핏 시점에서 운전자의 머리가 차량에 반응합니다: 코너, 제동, 미끄러짐, 속도.", "모션 블러",
     "모든 시점에서 모션 블러를 켜거나 끕니다."},
    /* CHT */
    {"動態視角", "在駕駛艙視角中，駕駛的頭部會隨車輛反應：過彎、煞車、甩尾與速度。", "動態模糊", "開啟或關閉所有視角的動態模糊。"},
    /* cz */ LABELS_EN,
    /* HU */ LABELS_EN,
    /* DEV */ LABELS_EN,
    /* LOC */ LABELS_EN,
};
