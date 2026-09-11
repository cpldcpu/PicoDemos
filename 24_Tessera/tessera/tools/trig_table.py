"""Reproduce the committed Q15 sine; no host/device libm difference at boot."""
import math
from pathlib import Path
values=[round(math.sin(i*math.tau/2048)*32767) for i in range(2048)]
text='/* Generated Q15 sine. tools/trig_table.py reproduces this table. */\nstatic const int16_t trig_q15[2048]={\n'
text+=''.join(' '+','.join(str(x) for x in values[i:i+16])+',\n' for i in range(0,2048,16))+'};\n'
(Path(__file__).resolve().parents[1]/'trig_table.h').write_text(text,encoding='utf-8')
