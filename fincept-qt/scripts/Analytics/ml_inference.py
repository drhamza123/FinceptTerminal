#!/usr/bin/env python3
"""ML Model Inference Bridge - called by MlSignalBridge C++ class.

Loads a trained model (LightGBM, XGBoost, sklearn) and runs inference.
Input: JSON with model_path, model_type, features array, feature_names.
Output: JSON with direction, confidence, prediction.

Usage:
  python ml_inference.py '{"model_path":"/path/to/model.pkl","model_type":"lightgbm","features":[1.2,3.4,...]}'
"""
import json
import sys
import os
import importlib.util

def load_model(model_path, model_type):
    """Load a trained model file."""
    if not os.path.exists(model_path):
        raise FileNotFoundError(f"Model not found: {model_path}")
    
    ext = os.path.splitext(model_path)[1].lower()
    
    if ext == '.pkl':
        import pickle
        with open(model_path, 'rb') as f:
            return pickle.load(f)
    elif ext == '.joblib':
        import joblib
        return joblib.load(model_path)
    elif ext == '.pt' or ext == '.pth':
        import torch
        return torch.jit.load(model_path)
    elif ext == '.onnx':
        import onnxruntime as ort
        return ort.InferenceSession(model_path)
    else:
        raise ValueError(f"Unsupported model format: {ext}")

def predict(model, model_type, features):
    """Run inference and return prediction."""
    import numpy as np
    
    X = np.array(features, dtype=np.float32).reshape(1, -1)
    
    if model_type in ('lightgbm', 'xgboost', 'sklearn'):
        pred = model.predict(X)[0]
        # Try to get probability/confidence
        confidence = 0.5
        try:
            if hasattr(model, 'predict_proba'):
                proba = model.predict_proba(X)[0]
                confidence = float(max(proba))
        except Exception:
            pass
        return float(pred), float(confidence)
    
    elif model_type == 'lstm' or model_type == 'transformer':
        import torch
        with torch.no_grad():
            if isinstance(model, torch.nn.Module):
                X_t = torch.tensor(X)
                if len(X_t.shape) == 1:
                    X_t = X_t.unsqueeze(0)
                output = model(X_t)
                pred = output.cpu().numpy()[0][0]
                return float(pred), 0.5
            return 0.0, 0.0
    
    return 0.0, 0.0

def main():
    if len(sys.argv) < 2:
        result = {"success": False, "error": "No input provided"}
        print(json.dumps(result))
        sys.exit(0)
    
    try:
        req = json.loads(sys.argv[1])
        model_path = req.get("model_path", "")
        model_type = req.get("model_type", "lightgbm")
        features = req.get("features", [])
        
        if not model_path:
            result = {"success": False, "error": "No model_path provided",
                       "direction": 0.0, "confidence": 0.0, "prediction": 0.0}
            print(json.dumps(result))
            return
        
        model = load_model(model_path, model_type)
        pred, confidence = predict(model, model_type, features)
        
        # Convert to trading signal
        direction = 0.0
        if pred > 0.01:
            direction = min(1.0, pred / 0.1)  # normalize
        elif pred < -0.01:
            direction = max(-1.0, pred / 0.1)
        
        result = {
            "success": True,
            "direction": direction,
            "confidence": confidence,
            "prediction": pred,
            "model_type": model_type,
        }
        print(json.dumps(result))
    
    except Exception as e:
        result = {"success": False, "error": str(e),
                   "direction": 0.0, "confidence": 0.0, "prediction": 0.0}
        print(json.dumps(result))

if __name__ == "__main__":
    main()
