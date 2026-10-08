FR — Essai iPhone 2 : compilateur récent
Ce projet ne contient PAS le jeu. Le premier essai a utilisé Apple Clang 17,
alors que le moteur Android exige Clang >=21. Cet essai utilise LLVM récent
pour compiler le vrai header port_ptr.h du moteur pour iOS ARM64.
Il vérifie la taille des pointeurs, les offsets, les accès aux tableaux,
les accès aux octets et la conversion des callbacks. Une compilation ARM64
Linux sert de contrôle : si Linux passe et iOS échoue, le problème dépend
probablement de la cible Darwin. Les rapports et l'IR seront conservés.
Apple Clang compile toujours l'interface UIKit de diagnostic.
Même un résultat positif ne résout pas le placement mémoire ni les callbacks
au-delà de 4 Gio, et ne signifie pas que le moteur complet fonctionne.
INSTALLATION GITHUB : conserver App/, tools/ et .github/workflows/.
Remplacer les fichiers existants du dépôt par ceux de cette archive.
Actions > iPhone toolchain attempt 2 > Run workflow.
Télécharger Compiler-results.zip et le transmettre pour analyse.
Pas besoin de réinstaller le diagnostic avant analyse du compilateur.
EN — Second iPhone attempt: recent compiler
This is NOT a playable game. Uses recent upstream LLVM with the real engine
pointer header for ARM64 iOS; Linux ARM64 is a comparison target.
Tests guest record size/offset, indexed stores, byte access and callback lowering.
UIKit still builds with Apple Clang. Successful compilation does not establish
runtime pointer correctness or solve memory placement and code dispatch.
Preserve the archive directory structure when uploading to GitHub.
Run Actions > iPhone toolchain attempt 2 and share Compiler-results.zip.
