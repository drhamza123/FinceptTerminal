import ast
import re
from pydantic import BaseModel, field_validator, Field
from typing import List, Optional
from datetime import datetime

BLOCKED_IMPORTS = {
    "os", "subprocess", "shutil", "signal", "ctypes", "multiprocessing",
    "socket", "ssl", "sys", "syslog", "tempfile", "threading",
}
BLOCKED_NODES = {
    ast.Call: lambda n: isinstance(n.func, ast.Attribute) and n.func.attr in {
        "eval", "exec", "compile", "__import__", "open", "input",
        "getattr", "setattr", "delattr",
    },
}

def validate_python_code(code: str) -> Optional[str]:
    try:
        tree = ast.parse(code)
    except SyntaxError as e:
        return f"Syntax error: {e}"
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                top = alias.name.split(".")[0]
                if top in BLOCKED_IMPORTS:
                    return f"Blocked import: {alias.name}"
        if isinstance(node, ast.ImportFrom):
            top = node.module.split(".")[0] if node.module else ""
            if top in BLOCKED_IMPORTS:
                return f"Blocked import from: {node.module}"
        for cls, check in BLOCKED_NODES.items():
            if isinstance(node, cls) and check(node):
                return f"Blocked function call at line {node.lineno}"
    return None

TOOL_NAME_RE = re.compile(r"^[a-zA-Z_][a-zA-Z0-9_]{2,48}$")

class McpToolPackage(BaseModel):
    id: str = Field(default="")
    name: str
    version: str = "1.0.0"
    author: str
    description: str
    category: str  # "Data", "Trading", "AI", "Utility"
    python_code: str
    dependencies: List[str] = Field(default_factory=list)
    security_rating: str = "UNVERIFIED"
    downloads: int = 0
    created_at: str = Field(default_factory=lambda: datetime.utcnow().isoformat())

    @field_validator("name")
    @classmethod
    def name_valid(cls, v: str) -> str:
        if not TOOL_NAME_RE.match(v):
            raise ValueError("Name must be 3-48 alphanumeric/underscore chars")
        return v

    @field_validator("python_code")
    @classmethod
    def code_safe(cls, v: str) -> str:
        err = validate_python_code(v)
        if err:
            raise ValueError(f"Security violation: {err}")
        return v

class AgentConfigPackage(BaseModel):
    id: str = ""
    name: str
    description: str
    llm_profile: str
    tools_enabled: List[str]
    system_prompt: str
    guardrails_config: dict = {}
    author: str
    downloads: int = 0
