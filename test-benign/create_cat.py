import re
from pathlib import Path
import pandas as pd

apps_text = """1Password
7+ Taskbar Tweaker
7z
7-Zip
ABBYY FineReader
Ableton Live
ACDSee
Acronis True Image
Action!
Adobe Acrobat Reader
Adobe Illustrator
Adobe Photoshop
Adobe Premiere Pro
Advanced Renamer
Advanced SystemCare
age
AIDA64
AIMP
AMD Adrenalin
Android Studio
ansible
ansible-playbook
Ant Renamer
AnyBurn
AnyDesk
AOMEI Backupper
Apache OpenOffice
Araxis Merge
aria2c
Atom
Audacity
Audition
Autodesk AutoCAD
AutoHotkey
AutoIt
autoruns
AVG Antivirus
Avira Antivirus
Bandicam
Bandizip
Battle.net
bazel
bcdedit
Beyond Compare
Bitdefender
BitTorrent Sync
Bitwarden
BleachBit
Blender
Bluebeam Revu
BlueStacks
Brackets
Brave
BS.Player
buck
Bulk Rename Utility
BurnAware
bzip2
Calibre
CamStudio
Camtasia
cargo
CCleaner
CDBurnerXP
certmgr
certreq
certutil
chkdsk
choco
Chromium
cipher
Citavi
cl
clang
Classic Shell
Claws Mail
ClipboardFusion
ClipX
cloudflared
cmake
cmd
Cobian Backup
Code::Blocks
Comodo Dragon
Comodo Internet Security
compact
CorelDRAW
Corsair iCUE
CPU-Z
CrystalDiskInfo
CrystalDiskMark
Cubase
curl
CutePDF
Cyberduck
CyberLink PowerDVD
Daemon Tools
Darktable
DaVinci Resolve
DBeaver
dcfldd
Deep Freeze
Dell SupportAssist
Deluge
Dev-C++
DiffMerge
Directory Opus
Discord
DiskGenius
diskpart
diskshadow
diskspd
dism
Ditto
docker
Docker Desktop
docker-compose
doPDF
Double Commander
Dropbox
dumpbin
EaseUS Partition Master
EaseUS Todo Backup
Eclipse IDE
eM Client
Emby
EmEditor
Epic Games Launcher
Epic Privacy Browser
ESET NOD32
Evernote
Everything
ewfacquire
Excel Viewer
FastCopy
FastStone Capture
FastStone Image Viewer
ffmpeg
Fiddler
FileZilla
find
findstr
fio
FL Studio
FolderSizes
foobar2000
Foxit PDF Reader
Foxit PhantomPDF
Foxmail
Fraps
FreeCommander
FreeFileSync
FreeMind
ftkimager
FurMark
g++
gcc
Geany
Gedit
GIMP
Git
git-clone
GitHub Desktop
GitKraken
git-lfs
Glary Utilities
GlassWire
go
GOG Galaxy
GoldWave
GOM Player
GoodSync
Google Chrome
Google Drive
gpg
GPU-Z
gradle
Greenshot
grpcurl
Guitar Pro
Guitar Rig
gzip
HandBrake
handle
Handy Backup
hashcat
hashdeep
HeidiSQL
helm
hg
HP Support Assistant
httpie
HWiNFO
HWMonitor
HxD
Hyper-V Manager
icacls
IDA
imagemagick
imdisk
ImgBurn
InfraRecorder
Inkscape
Insomnia
Intel Driver & Support Assistant
iperf
iperf3
IrfanView
iTunes
jar
java
javac
JetBrains CLion
JetBrains IntelliJ IDEA
JetBrains PyCharm
JetBrains WebStorm
john
Joplin
Kaspersky
Kate
Kdenlive
KeePass
KeePassXC
keytool
KMPlayer
Kodi
Kontakt
Krita
kubectl
Kubernetes Dashboard
LastPass
Lenovo Vantage
LibreOffice Calc
LibreOffice Impress
LibreOffice Writer
LibreWolf
Lightshot
Lightworks
link
LMMS
Logitech G Hub
logman
Macrium Reflect
Mailbird
make
makecert
Malwarebytes
MariaDB
Maxthon
McAfee security
md5sum
Media Player Classic
MediaMonkey
MegaSync
MemTest86
Mendeley
meson
Microsoft Edge
Microsoft Excel
Microsoft OneNote
Microsoft Outlook
Microsoft PowerPoint
Microsoft Teams
Microsoft Word
Midori
MindMeister
minisign
MiniTool Partition Wizard
mitmproxy
Mixcraft
MobaXterm
mongo
MongoDB Compass
mongodump
mongorestore
more
mountvol
Mozilla Firefox
Mp3tag
msbuild
MSI Afterburner
Multi Commander
MusicBee
mvn
MySQL
net
NetBeans
netcat
NetLimiter
netstat
ngrok
ninja
Nitro PDF Reader
Nitro Pro
nmake
nmap
node
Notepad++
Notepad3
Notion
npm
nuget
NVIDIA GeForce Experience
NZXT CAM
OBS Studio
OBS VirtualCam
Obsidian
OCCT
OneDrive
OnlyOffice Desktop
Open-Shell
OpenShot
openssl
Opera
Opera Mail
Origin
packer
Paint.NET
PaintTool SAI
Pale Moon
Paragon Partition Manager
Parallels Desktop
Password Safe
pCloud
PDFCreator
PDF-XChange Editor
PeaZip
perl
pg_dump
pg_restore
pgAdmin
php
pip
pipenv
PlayClaw
Plex
plink
pnpm
poetry
PostgreSQL
Postman
PotPlayer
PowerISO
PowerPoint Viewer
powershell
PowerToys
Prime95
PrivaZer
Process Explorer
Process Hacker
procexp
procmon
pscp
psexec
pskill
pslist
psql
psshutdown
pssuspend
PuTTY
pwsh
python
qBittorrent
QEMU
Qt Creator
Rainlendar
Rainmeter
rammap
RawTherapee
Razer Cortex
rclone
Reaper
Reason
Redis Desktop Manager
redis-cli
reg
Resilio Sync
Revo Uninstaller
RJ TextEd
robocopy
ruby
Rufus
rustc
Samsung Magician
Sandboxie
sc
schtasks
scons
scoop
ScreenToGif
SeaMonkey
sfc
sha1sum
sha256sum
Shadow Defender
ShareX
Shotcut
sigcheck
Signal Desktop
signtool
SketchUp
Skype
Slack
Slimjet
SMPlayer
Snagit
socat
Sophos Home
sort
SourceTree
SpaceSniffer
Speccy
Spotify
sqlite3
SQLiteStudio
SRWare Iron
Steam
SteelSeries GG
stress
stress-ng (WSL)
strings
Sublime Text
SumatraPDF
svn
Sylpheed
SyncBack
sysbench
TagScanner
takeown
tar
taskkill
tasklist
tcpdump
tcpview
TeamViewer
Telegram Desktop
TeraCopy
terraform
TextPad
Thunderbird
Time Freeze
timeout
Tor Browser
TortoiseGit
TortoiseSVN
Total Commander
Transmission
TreeSize
Trello Desktop
TrueCrypt
tshark
Typora
UC Browser
UltraEdit
uTorrent
vagrant
VeraCrypt
VirtualBox
VirtualBox Extension Pack
VirtualDJ
VirtualDub
Visual Studio
Visual Studio Code
Vivaldi
VLC Media Player
VMware Player
VMware Workstation
Waterfox
WavePad
wbadmin
wevtutil
wget
WhatsApp Desktop
WinDirStat
winget
WinHex
WinMerge
WinRAR
winrs
WinSCP
WinZip
Wireshark
Wise Care 365
Wise Disk Cleaner
wmic
Word Viewer
WPS Office
XAMPP
xcopy
XMind
XnView
XSplit
XYplorer
xz
Yandex Browser
yarn
ZoneAlarm
Zoner Photo Studio
Zoom
Zotero
"""
apps = [line.strip() for line in apps_text.splitlines() if line.strip()]

# Category sets (lowercased for matching)
password_mgr = {"1password","bitwarden","keepass","keepassxc","lastpass","password safe"}
browsers = {"google chrome","microsoft edge","mozilla firefox","brave","opera","vivaldi","chromium","tor browser",
            "waterfox","palemoon","pale moon","librewolf","maxthon","midori","seamonkey","slimjet","srware iron",
            "epic privacy browser","yandex browser","uc browser","comodo dragon"}
office_suites = {"microsoft word","microsoft excel","microsoft powerpoint","microsoft outlook","microsoft onenote",
                 "libreoffice writer","libreoffice calc","libreoffice impress","apache openoffice","wps office",
                 "onlyoffice desktop","word viewer","excel viewer","powerpoint viewer"}
pdf_tools = {"adobe acrobat reader","foxit pdf reader","foxit phantompdf","nitro pdf reader","nitro pro",
             "sumatrapdf","pdf-xchange editor","pdfcreator","dopdf","cutepdf","bluebeam revu"}
archivers = {"7z","7-zip","7-zip","winrar","winzip","peazip","bandizip","tar","gzip","bzip2","xz"}
backup_sync = {"acronis true image","aomei backupper","easeus todo backup","macrium reflect","cobian backup",
               "handy backup","goodSync".lower(),"freefilesync","syncback","resilio sync","bittorrent sync",
               "dropbox","google drive","onedrive","megasynC".lower(),"pcloud","rclone","robocopy","teracopy"}
disk_partition = {"easeus partition master","minitool partition wizard","diskgenius","paragon partition manager",
                  "wbadmin","diskpart","diskshadow","chkdsk","sfc","dism","mountvol","bcdedit"}
remote_access = {"anydesk","teamviewer","putty","mobaxterm","plink","pscp","psexec"}
p2p = {"qbittorrent","utorrent","transmission","deluge"}
media_players = {"vlc media player","potplayer","kmplayer","gom player","media player classic","kodi","plex","emby",
                 "musicbee","foobar2000","aimp","bs.player","smplayer","itunes","spotify","mediamonkey"}
screen_capture = {"obs studio","obs virtualcam","bandicam","camtasia","camstudio","fraps","action!","snagit",
                  "greenshot","sharex","lightshot","screentogif","faststone capture","xsplit","playclaw"}
video_edit = {"adobe premiere pro","davinci resolve","lightworks","openshot","shotcut","kdenlive","virtualdub",
              "handbrake"}
audio_prod = {"ableton live","fl studio","cubase","reaper","reason","mixcraft","lmms","audition","audacity",
              "guitar pro","guitar rig","kontakt","goldwave","wavepad"}
image_graphics = {"adobe photoshop","adobe illustrator","gimp","krita","inkscape","coreldraw","paint.net","painttool sai",
                  "blender","darktable","rawtherapee","irfanview","faststone image viewer","acdsee","xnview",
                  "zoner photo studio","imagemagick","sketchup","autodesk autocad"}
dev_ides = {"visual studio","visual studio code","android studio","eclipse ide","netbeans","qt creator","code::blocks",
            "dev-c++","geany","gedit","kate","brackets","atom","sublime text","ultraedit","notepad++","notepad3",
            "rj texted","textpad","typora"}
dev_build = {"gcc","g++","clang","cl","cmake","make","nmake","ninja","msbuild","mvn","gradle","bazel","buck",
             "cargo","rustc","go","node","npm","yarn","pnpm","nuget","pip","pipenv","poetry","packer","terraform","vagrant","scons","meson"}
dev_vcs = {"git","git-clone","git-lfs","github desktop","gitkraken","sourcetree","tortoisegit","svn","tortoisesvn","hg"}
devops_container = {"docker","docker desktop","docker-compose","kubectl","kubernetes dashboard","helm","xampP".lower()}
db_tools = {"mysql","postgresql","mariadb","mongo","mongodb compass","sqlite3","sqlitestudio","dbeaver","heidisql","pgadmin",
            "psql","pg_dump","pg_restore","mongodump","mongorestore","redis-cli","redis desktop manager"}
net_tools = {"curl","wget","filezilla","cyberduck","fiddler","mitmproxy","nmap","netcat","iperf","iperf3","ngrok",
             "cloudflared","httpie","grpcurl","socat","tcpdump","tshark","wireshark","netstat","net","tcpview"}
email_chat = {"discord","slack","telegram desktop","whatsapp desktop","signal desktop","skype","zoom","microsoft teams",
              "thunderbird","mailbird","em client","foxmail","opera mail","claws mail","sylpheed"}
games = {"steam","origin","battle.net","epic games launcher","gog galaxy","razer cortex"}
security_av = {"kaspersky","eset nod32","mcafee security","malwarebytes","bitdefender","avira antivirus","avg antivirus",
               "sophos home","zonealarm","comodo internet security"}
sys_cleanup = {"ccleaner","bleachbit","wise care 365","wise disk cleaner","glary utilities","advanced systemcare",
               "privaZer".lower(),"revo uninstaller"}
sys_monitor = {"aida64","cpu-z","gpu-z","hwinfo","hwmonitor","crystaldiskinfo","crystaldiskmark","furmark","prime95",
               "occt","memtest86","speccy","msi afterburner","nzxt cam","samsung magician","corsair icue","logitech g hub",
               "steelseries gg","nvidia geforce experience","amd adrenalin","intel driver & support assistant",
               "dell supportassist","hp support assistant","lenovo vantage"}
sysinternals = {"autoruns","process explorer","process hacker","procmon","procexp","rammap","sigcheck","handle"}
file_mgr = {"total commander","directory opus","double commander","freecommander","multi commander","xyplorer","everything",
            "spacesniffer","treesize","windirstat","foldersizes"}
rename_tools = {"advanced renamer","ant renamer","bulk rename utility","fastcopy"}  # fastcopy also copy tool but ok
disc_iso = {"imgburn","cdburnerxp","burnaware","anyburn","infrarecorder","poweriso","daemon tools","imdisk","rufus"}
forensics_re = {"ida","winhex","hxd","ftkimager","hashcat","hashdeep","john","dcfldd","ewfacquire","strings","signtool",
                "certutil","certreq","certmgr","makecert"}

windows_cli = {"cmd","powershell","pwsh","reg","schtasks","sc","taskkill","tasklist","takeown","icacls","wevtutil","logman",
               "find","findstr","cipher","compact","sort","more","timeout","wmic","xcopy","winget","choco","scoop","netstat","wbadmin"}
crypto_tools = {"gpg","openssl","age","minisign","sha1sum","sha256sum","md5sum","keytool"}

notes_prod = {"evernote","onenote","microsoft onenote","notion","obsidian","joplin","trello desktop","mindmeister","xmind","freemind","citavi","mendeley","zotero","rainlendar","rainmeter"}

virtualization = {"virtualbox","virtualbox extension pack","vmware player","vmware workstation","hyper-v manager","qemu","parallels desktop","sandboxie","shadow defender","deep freeze","time freeze","truecrypt","veracrypt"}

misc_explicit = {"7+ taskbar tweaker","classic shell","open-shell","powertoys","clipboardfusion","clipx","ditto","clipx",
                 "notepad3"}

def categorize(app):
    a = app.strip().lower()

    if a in password_mgr:
        return "Password Manager"
    if a in browsers:
        return "Browser"
    if a in office_suites:
        return "Office / Productivity"
    if a in pdf_tools:
        return "PDF Tools"
    if a in archivers:
        return "Archiver / Compression"
    if a in backup_sync or a in {"goodsync","megasync"}:
        return "Backup / Sync"
    if a in disk_partition:
        return "Disk / Partition / Recovery"
    if a in remote_access:
        return "Remote Access / SSH"
    if a in p2p:
        return "Torrent / P2P"
    if a in media_players:
        return "Media Player / Library"
    if a in screen_capture:
        return "Screen Capture / Recording"
    if a in video_edit:
        return "Video Editing / Transcoding"
    if a in audio_prod:
        return "Audio Production / Editing"
    if a in image_graphics:
        return "Graphics / CAD / Design"
    if a in dev_ides:
        return "Dev: IDE / Editor"
    if a in dev_build:
        return "Dev: Build / Toolchain"
    if a in dev_vcs:
        return "Dev: Version Control"
    if a in devops_container or a in {"xampp"}:
        return "DevOps: Container / K8s / Stack"
    if a in db_tools:
        return "Database Tools"
    if a in net_tools:
        return "Network Tools"
    if a in email_chat:
        return "Email / Chat / Collaboration"
    if a in games:
        return "Game Launcher / Store"
    if a in security_av:
        return "Security: Antivirus"
    if a in sys_cleanup:
        return "System Cleanup / Uninstaller"
    if a in sys_monitor:
        return "Hardware Monitoring / Drivers"
    if a in sysinternals:
        return "System Monitoring / Sysinternals"
    if a in file_mgr:
        return "File Manager / Disk Usage"
    if a in rename_tools:
        return "File Rename / Copy Utility"
    if a in disc_iso:
        return "Disc / ISO / USB Tools"
    if a in forensics_re:
        return "Forensics / Reverse Engineering"
    if a in windows_cli:
        return "Windows System Tools (CLI)"
    if a in crypto_tools:
        return "Security: Crypto Tools"
    if a in notes_prod:
        return "Notes / Reference / Mindmap"
    if a in virtualization:
        return "Virtualization / Sandbox / Encryption"
    if a in misc_explicit:
        return "System Tweaks / Utilities"

    # Some keyword heuristics
    if "launcher" in a:
        return "Game Launcher / Store"
    if "pdf" in a:
        return "PDF Tools"
    if "office" in a:
        return "Office / Productivity"
    if "antivirus" in a or "security" in a:
        return "Security: Antivirus"
    if "driver" in a or "adrenalin" in a or "geforce" in a:
        return "Hardware Monitoring / Drivers"
    if "merge" in a or "compare" in a or "diff" in a:
        return "Dev: Diff / Merge"
    if "burn" in a or "iso" in a:
        return "Disc / ISO / USB Tools"
    if a.endswith(".exe"):
        return "Misc"

    return "Misc"

rows = [{"Category": categorize(app), "App": app} for app in apps]
df = pd.DataFrame(rows)

# Sort for readability
df.sort_values(["Category", "App"], inplace=True, ignore_index=True)

out_path = Path("apps_by_category.csv")

df.to_csv(
    out_path,
    index=False,
    encoding="utf-8",
)

print(out_path.as_posix(), df.shape, df["Category"].nunique())
