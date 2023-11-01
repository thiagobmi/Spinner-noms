while :; do
    regHex="$(/opt/netronome/p4/bin/rtecli -p 20206 registers get -r debug_reg | tr '\n' ' ')"
    echo "[1]:"$(( 16#${regHex:4:8} ))" [2]:"$(( 16#${regHex:19:8} ))" [3]:"$(( 16#${regHex:34:8} ))" [4]:"$(( 16#${regHex:49:8} ))" [5]:"$(( 16#${regHex:64:8} ))" [6]:"$(( 16#${regHex:79:8} ))" [7]:"$(( 16#${regHex:94:8} ))" [8]:"$(( 16#${regHex:109:8} ))" [9]:"$(( 16#${regHex:124:8} ))" [10]:"$(( 16#${regHex:139:8} ))



done
