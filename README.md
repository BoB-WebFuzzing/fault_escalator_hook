# gcc -shared -o hook_recv.so testhook.c -ldl -fPIC

# LD_PRELOAD=./hook_recv.so node app.js