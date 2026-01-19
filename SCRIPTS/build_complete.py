from __future__ import annotations
import sys

def beep(*, success: bool = True) -> None:
    """Play an audible notification. Best-effort cross-platform."""
    try:
        if sys.platform.startswith("win"):
            import winsound
            winsound.Beep(880 if success else 220, 120 if success else 400)
        else:
            sys.stdout.write("\a")
            sys.stdout.flush()
    except Exception:
        pass
