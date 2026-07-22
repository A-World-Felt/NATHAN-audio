import sofa
import numpy as np

hrtf = sofa.Database.open(r'C:\Users\micst\Documents\nathan-audio\assets\hrtf\subject_003.sofa')
pos = hrtf.Source.Position.get_values()

azimuths   = np.unique(np.round(pos[:, 0], 1))
elevations = np.unique(np.round(pos[:, 1], 1))

print(f'Azimuths uniques ({len(azimuths)}) : {azimuths}')
print(f'Elevations uniques ({len(elevations)}) : {elevations}')
