mkdir deploy_web
copy build_web\seerohr.html deploy_web\seerohr.html
copy build_web\seerohr.js deploy_web\seerohr.js
copy build_web\seerohr.wasm deploy_web\seerohr.wasm
robocopy assets deploy_web\assets /mir

