def compute_metrics(tp, fn, tn, fp):
    tpr = tp / (tp + fn) if (tp + fn) else 0
    tnr = tn / (tn + fp) if (tn + fp) else 0
    fnr = fn / (tp + fn) if (tp + fn) else 0
    fpr = fp / (fp + tn) if (fp + tn) else 0
    accuracy = (tp + tn) / (tp + tn + fp + fn) if (tp + tn + fp + fn) else 0
    precision = tp / (tp + fp) if (tp + fp) else 0
    f1 = 2 * precision * tpr / (precision + tpr) if (precision + tpr) else 0

    return {
        "TPR": round(tpr * 100, 2),
        "TNR": round(tnr * 100, 2),
        "FNR": round(fnr * 100, 2),
        "FPR": round(fpr * 100, 2),
        "Accuracy": round(accuracy * 100, 2),
        "Precision": round(precision * 100, 2),
        "F1": round(f1 * 100, 2)
    }

# Dữ liệu đầu vào
data = {
    "Rejected Ext": {
        "enc_total": 291800,
        "enc_detected": 279917,
        "clean_total": 424394,
        "clean_detected": 15205
    },
    "Null Ext": {
        "enc_total": 291800,
        "enc_detected": 264616,
        "clean_total": 424394,
        "clean_detected": 15071
    },
    "Shannon High": {
        "enc_total": 291800,
        "enc_detected": 247902,
        "clean_total": 424394,
        "clean_detected": 197272
    },
    "Chi-Square Low": {
        "enc_total": 291800,
        "enc_detected": 202974,
        "clean_total": 424394,
        "clean_detected": 43441
    }
}

# Tính toán
results = {}
for method, val in data.items():
    TP = val["enc_detected"]
    FN = val["enc_total"] - TP
    FP = val["clean_detected"]
    TN = val["clean_total"] - FP

    results[method] = compute_metrics(TP, FN, TN, FP)

# In kết quả
from tabulate import tabulate
headers = ["Method", "TPR (%)", "TNR (%)", "FNR (%)", "FPR (%)", "Accuracy (%)", "Precision (%)", "F1 (%)"]
table = [
    [method] + list(metrics.values())
    for method, metrics in results.items()
]
print(tabulate(table, headers=headers, tablefmt="github"))
