import os

from collections import defaultdict

type_cnt = defaultdict(int)

try:
    with open(r'E:\Graduation_Project\custom_dataset\code\report\Phobos.txt', "r", encoding="utf-8") as f:
        lines = [line.strip().lower() for line in f]
    with open(r'E:\Graduation_Project\custom_dataset\code\report_napier_encrypted\Phobos.txt', "r", encoding="utf-8") as f:
        lines.extend([line.strip().lower() for line in f])

    for i in range(0, len(lines), 2):
        if lines[i+1] == "":
            continue
        type_cnt[lines[i+1]] += 1
        '''
        types = list()
        for type in lines[i+1].split(";"):
            if type not in types:
                types.append(type)
        for type in types:
            type_cnt[type] += 1
        '''        
except Exception as e:
    pass

total = sum([cnt if "null" not in type else 0 for type, cnt in type_cnt.items()])
print(f"Total: {total}")

for type, cnt in type_cnt.items():
    #print(f"{type}: {cnt}") 
    pass