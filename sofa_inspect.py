import sofa
import numpy as np
hrtf = sofa.Database.open(r'C:\Users\micst\Documents\nathan-audio\assets\hrtf\subject_003.sofa')
print('Sample rate:', hrtf.Data.SamplingRate.get_values())
print('IR shape:', hrtf.Data.IR.get_values().shape)
print('Source positions shape:', hrtf.Source.Position.get_values().shape)
print('Source positions (5 premiers):', hrtf.Source.Position.get_values()[:5])
