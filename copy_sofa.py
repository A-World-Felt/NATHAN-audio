import os
appdata = os.environ['APPDATA']
hrtf_dir = os.path.join(appdata, 'OpenAL', 'hrtf')
os.makedirs(hrtf_dir, exist_ok=True)

import shutil
src = r'C:\Users\micst\Documents\nathan-audio\assets\hrtf'
for f in os.listdir(src):
    if f.endswith('.sofa'):
        shutil.copy(os.path.join(src, f), os.path.join(hrtf_dir, f))
        print(f'Copie : {f}')

print(f'Done — {hrtf_dir}')
