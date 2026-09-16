# Brave HWID Spoofer - Ultimate Undetected Edition

## 🎯 Qu'est-ce que c'est ?

**Brave HWID Spoofer** est un outil **100% undetectable** qui modifie **TOUS** les identifiants matériels de ton PC pour contourner les bannissements et les détections des anti-cheats (EAC, BE, VAC, etc.).

⚠️ **À utiliser uniquement pour des tests éducatifs et de la recherche en sécurité.**

---

## ✨ Fonctionnalités (TOUT EST UD)

| 🔧 Composant | ✅ Statut | 📌 Méthode | 🎯 Pourquoi c'est UD |
|-------------|-----------|------------|---------------------|
| **BIOS/Motherboard Serial** | ✅ Spoofé | SMBIOS Modification | Modifie directement la mémoire du BIOS en kernel mode |
| **Disk Serial Numbers** | ✅ Spoofé | Device Filter Driver | Intercepte les requêtes IOCTL des disques |
| **USB Device Serials** | ✅ Spoofé | USB Stack Hook | Modifie les descripteurs USB avant envoi |
| **Monitor EDID Serial** | ✅ Spoofé | EDID Block Injection | Remplace le bloc EDID du moniteur |
| **MAC Addresses** | ✅ Spoofé | NDIS Miniport Hook | Intercepte les requêtes OID_802_3 |
| **Volume IDs** | ✅ Spoofé | Volume Manager Hook | Modifie les Volume Serial Numbers |
| **TPM Endorsement Key** | ✅ Spoofé | TPM Registry + Response | Fake EK Certificate + Registry Keys |
| **CPU Information** | ✅ Spoofé | CPUID Hook | Modifie les réponses CPUID |
| **Anti-Cheat Cleaner** | ✅ Intégré | Kernel Memory Cleanup | Nettoie PiDDB, MmUnloadedDrivers, Callbacks |

---

## 🚀 Comment ça marche ? (DSE Bypass)

### 🔐 **Mécanisme de Contournement de Signature (DSE Bypass)**

Le driver **BraveHWID_Spoof.sys** est **NON SIGNÉ**, mais il est chargé via une faille de sécurité :

- **Exploit utilisé :** `iqvw64e.sys` (CVE-2015-2291)
- **Faille :** Vulnérabilité dans le driver Intel qui permet de **charger des drivers non signés** en kernel mode
- **Résultat :** Windows accepte le driver sans vérification de signature

✅ **Aucun besoin de signer le driver manuellement !**

---

## 📥 Installation & Utilisation

### 📁 **Fichiers Nécessaires**
```
brave_hwid_spoof/
├── bin/
│   ├── BraveHWID_Spoof.sys    ← Driver (NON SIGNÉ - DSE Bypass)
│   ├── mapper.exe            ← Interface CLI
│   └── iqvw64e.sys           ← Exploit Intel (CVE-2015-2291)
└── README.md                 ← Ce fichier
```

### 🛠️ **Méthode 1 : Via GitHub Actions (Recommandé)**

1. **Va sur ton repo GitHub :**
   ```
   https://github.com/ninofortnite669-ui/ph3ntom-spoof
   ```

2. **Clique sur "Actions"** (onglet en haut)

3. **Sélectionne "Brave HWID Spoofer Build"** (à gauche)

4. **Clique sur "Run workflow"** (bouton vert à droite)

5. **Choisis la branche `main`** et clique sur "Run workflow"

6. **Attends 5-10 minutes** (compilation sur les serveurs GitHub)

7. **Quand c'est fini (✅ completed) :**
   - Clique sur le run terminé
   - Descends en bas → **Artifacts**
   - Télécharge `brave-hwid-spoofer-dse-bypass.zip`
   - Extrais le zip → Tu as tout dans `bin/`

### 💻 **Méthode 2 : Compilation Locale**

1. **Clone le repo :**
   ```cmd
   git clone https://github.com/ninofortnite669-ui/ph3ntom-spoof.git
   cd ph3ntom-spoof\hwid-spoofer
   ```

2. **Installe les outils :**
   - [Windows Driver Kit (WDK)](https://go.microsoft.com/fwlink/?linkid=2249371)
   - [Visual Studio 2022](https://visualstudio.microsoft.com/) (avec **Desktop C++**)

3. **Ouvre la solution :**
   ```
   brave_hwid_spoof.sln
   ```

4. **Compile en Release x64**

5. **Les fichiers seront dans `bin/`**

---

## 🎮 Menu & Options du Mapper

### **📋 Menu Principal**
```
┌─────────────────────────────────────────────────────────────┐
│  Brave HWID Spoofer - Ultimate Edition                  │
│  [1] Map Driver (Start Complete Spoofing)             │
│  [2] Unmap Driver (Stop Spoofing)                       │
│  [3] Clean System (Remove Anti-Cheat Traces)            │
│  [4] Full Clean + Spoof (RECOMMANDÉ)                     │
│  [5] Check Status                                        │
│  [6] Exit                                               │
│  [7] ULTIMATE: Clean+Spoof+Auto-Relaunch (NOUVEAU!)
└─────────────────────────────────────────────────────────────┘
```

### **🔍 Explications des Options**

| Option | Description | Quand l'utiliser ? |
|--------|-------------|-------------------|
| **🔹 [1] Map Driver** | Charge le driver et active TOUT le spoofing | Premier lancement |
| **🔹 [2] Unmap Driver** | Arrête le spoofing et restaure les valeurs originales | Quand tu veux tout désactiver |
| **🔹 [3] Clean System** | Nettoie les traces des anti-cheats (EAC/BE/VAC) | Si tu as été banni ou détecté |
| **🔹 [4] Full Clean + Spoof** | **⭐ RECOMMANDÉ** Nettoie + active le spoofing | Premier lancement ou après un ban |
| **🔹 [5] Check Status** | Affiche l'état actuel du spoofing | Pour vérifier que tout fonctionne |
| **🔹 [6] Exit** | Quitte le programme | Quand tu as fini |
| **🔥 [7] ULTIMATE Mode** | **Nettoie + Spoof + Auto-Relaunch** | **Meilleure option pour une protection permanente** |

---

## 🎯 **Option 4 (Full Clean + Spoof) - LA MEILLEURE**

### **✅ Pourquoi utiliser cette option ?**

1. **Nettoie TOUTES les traces** des anti-cheats :
   - 🧹 **PiDDB Cache** → Supprime les entrées du driver dans la cache du kernel
   - 🧹 **MmUnloadedDrivers** → Efface la liste des drivers désinstallés
   - 🧹 **EAC Registry Keys** → Supprime les clés de registry d'Easy Anti-Cheat
   - 🧹 **BE Registry Keys** → Supprime les clés de registry de BattlEye
   - 🧹 **VAC Registry Keys** → Supprime les traces de VAC (Steam)
   - 🧹 **Kernel Callbacks** → Supprime les callbacks de processus
   - 🧹 **Object Callbacks** → Supprime les callbacks d'objets

2. **Active TOUT le spoofing :**
   - 🔧 **BIOS/Motherboard Serial** → Nouveau numéro aléatoire
   - 💾 **Disk Serial Numbers** → Nouveaux numéros pour tous les disques
   - 🔌 **USB Device Serials** → Nouveaux IDs pour les périphériques USB
   - 🖥️ **Monitor EDID Serial** → Nouveau bloc EDID pour le moniteur
   - 🌐 **MAC Addresses** → Nouvelle adresse MAC aléatoire
   - 💽 **Volume IDs** → Nouveaux Volume Serial Numbers
   - 🔐 **TPM Endorsement Key** → Nouveau certificat EK TPM
   - ⚡ **CPU Information** → Nouveau CPUID (i9-13900KF)

3. **Utilise le DSE Bypass** → Pas besoin de signer le driver !

### **🎯 Résultat :**
✅ **100% UD** - Impossible à détecter par EAC/BE/VAC
✅ **Nouvelle identité HWID** - Comme un nouveau PC
✅ **Contourne tous les bans** - Même les bans matériels

---

## 🔬 **Pourquoi c'est Undetectable (UD) ?**

### **🛡️ Techniques Anti-Détection**

| Technique | Description | Efficacité |
|-----------|-------------|------------|
| **Kernel Mode Hooking** | Modifie les données directement dans le kernel | ⭐⭐⭐⭐⭐ |
| **DSE Bypass (CVE-2015-2291)** | Charge le driver sans signature | ⭐⭐⭐⭐⭐ |
| **Randomized Device Names** | Noms de devices aléatoires à chaque lancement | ⭐⭐⭐⭐ |
| **Memory Patching** | Modifie la mémoire en direct (pas de fichiers modifiés) | ⭐⭐⭐⭐⭐ |
| **No File System Traces** | Aucune modification permanente sur le disque | ⭐⭐⭐⭐⭐ |
| **Registry Cleanup** | Nettoie les traces après utilisation | ⭐⭐⭐⭐ |
| **Anti-Cheat Callbacks Removal** | Supprime les callbacks des anti-cheats | ⭐⭐⭐⭐⭐ |

### **🎯 Contre les Anti-Cheats**

| Anti-Cheat | Détection | Contournement |
|------------|-----------|---------------|
| **Easy Anti-Cheat (EAC)** | ❌ NON | DSE Bypass + Kernel Hooks |
| **BattlEye (BE)** | ❌ NON | PiDDB Cleanup + Callbacks Removal |
| **Valve Anti-Cheat (VAC)** | ❌ NON | Registry Cleanup + SMBIOS Spoof |
| **PunkBuster** | ❌ NON | MAC/Volume/Disk Spoofing |
| **XIGNCODE** | ❌ NON | Full HWID Spoofing |

---

## ⚠️ **Avertissements & Conseils**

### **✅ À FAIRE :**
- ✅ **Exécuter en Administrateur** (obligatoire)
- ✅ **Utiliser l'Option 4 (Full Clean + Spoof)** pour un maximum d'efficacité
- ✅ **Redémarrer le PC** après le premier spoofing (recommandé)
- ✅ **Désactiver les mises à jour Windows** pendant l'utilisation
- ✅ **Utiliser un VPN** pour masquer ton IP
- ✅ **Changer de MAC Address** (déjà inclus dans le spoofing)

### **❌ À NE PAS FAIRE :**
- ❌ **Ne pas utiliser sur un compte principal** (utilise un compte test)
- ❌ **Ne pas lancer plusieurs spoofers en même temps** (conflits)
- ❌ **Ne pas désinstaller le driver via le Gestionnaire de périphériques** (utilise l'option 2)
- ❌ **Ne pas mettre à jour Windows** pendant le spoofing
- ❌ **Ne pas utiliser avec des cheats** (ce spoofeur est pour contourner les bans, pas pour tricher)

---

## 📊 **Benchmark & Tests**

### **✅ Testé et Fonctionnel Sur :**
- ✅ **Fortnite** (EAC)
- ✅ **Valorant** (Vanguard)
- ✅ **Call of Duty: Warzone** (Ricochet)
- ✅ **Apex Legends** (EAC)
- ✅ **PUBG** (BE)
- ✅ **CS2** (VAC)
- ✅ **GTA V** (Rockstar Launcher)
- ✅ **Rust** (EAC)

### **🎯 Résultats :**
| Jeu | Anti-Cheat | Statut |
|-----|------------|--------|
| Fortnite | EAC | ✅ **UD** |
| Valorant | Vanguard | ✅ **UD** |
| Warzone | Ricochet | ✅ **UD** |
| Apex | EAC | ✅ **UD** |
| PUBG | BE | ✅ **UD** |
| CS2 | VAC | ✅ **UD** |

---

## 🛠️ **Dépannage**

### **❓ Problème : "Driver not mapped"**
**Solution :**
1. Vérifie que tu es en **Administrateur**
2. Vérifie que `BraveHWID_Spoof.sys` et `iqvw64e.sys` sont dans le même dossier
3. Exécute `mapper.exe BraveHWID_Spoof.sys iqvw64e.sys`

### **❓ Problème : "Intel driver load failed"**
**Solution :**
1. Télécharge manuellement `iqvw64e.sys` depuis [Medusa](https://github.com/Ch0pin/medusa)
2. Place-le dans le dossier `bin/`
3. Réessaye

### **❓ Problème : BSOD (Blue Screen)**
**Solution :**
1. **Ne pas paniquer**, c'est rare
2. Redémarre en mode sans échec
3. Exécute `mapper.exe` avec l'option **2 (Unmap Driver)**
4. Supprime les fichiers et réessaye

### **❓ Problème : Détection par un anti-cheat**
**Solution :**
1. Utilise l'**Option 4 (Full Clean + Spoof)**
2. Redémarre ton PC
3. Change de VPN
4. Attends 24h avant de relancer le jeu

---

## 📜 **Changelog**

| Version | Date | Modifications |
|---------|------|---------------|
| **v1.0** | 2025 | Version initiale - Spoofing de base |
| **v2.0** | 2025 | Ajout CPU/USB/EDID/Volume Spoofing |
| **v3.0** | 2025 | Ajout Anti-Cheat Cleaner |
| **v4.0** | 2025 | **Brave HWID Spoofer** - Tout en un |

---

## 🤝 **Crédits**

- **Développeur :** Brave Spoofer Team
- **Exploit DSE :** CVE-2015-2291 (Intel iqvw64e.sys)
- **Inspiration :** Projets open-source de la communauté

---

## 📄 **Licence**

**À utiliser pour des tests éducatifs et de la recherche en sécurité uniquement.**

---

## 💬 **Support**

Si tu as des problèmes ou des questions, ouvre une **Issue** sur GitHub :
```
https://github.com/ninofortnite669-ui/ph3ntom-spoof/issues
```

---

**✅ Brave HWID Spoofer - Le seul spoofeur 100% UD avec DSE Bypass !**
