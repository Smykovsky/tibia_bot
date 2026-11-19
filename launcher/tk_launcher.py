#!/usr/bin/env python3
"""
TibiaBot – Shrouded Edition (Universal GUI Launcher)
Tabbed Bot Interface inspired by Shrouded Bot, Xenobot and modern bot suites.
Zero external dependencies required (native Python 3 + Tkinter).
"""

import os
import sys

# Silence macOS Tk deprecation notice
os.environ["TK_SILENCE_DEPRECATION"] = "1"

# If PySide6 is installed in .venv, automatically launch the modern PySide6 / Qt launcher
_root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_venv_python = os.path.join(_root_dir, ".venv", "bin", "python")
_main_py = os.path.join(_root_dir, "launcher", "main.py")

if "--tk" not in sys.argv and os.path.exists(_venv_python) and os.path.exists(_main_py):
    _args = [a for a in sys.argv[1:] if a != "--tk"]
    os.execv(_venv_python, [_venv_python, _main_py, "--auto-start"] + _args)

import json
import time
import threading
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext

# Add project root to sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from launcher.core.engine_client import EngineClient
from launcher.core.engine_process import EngineProcessManager

class TibiaBotTkLauncher:
    def __init__(self, root, port=8765):
        self.root = root
        self.port = port
        self.root.title("TibiaBot – Shrouded Edition")
        self.root.geometry("1180x760")
        self.root.minsize(980, 640)
        self.root.configure(bg="#0b0f19")

        self.configs_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "configs"))
        self.scripts_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "scripts"))

        self.engine_proc = EngineProcessManager(port=self.port)
        self.client = EngineClient(port=self.port)
        self.client.on_log_callback = self.on_log_received
        self.client.on_event_callback = self.on_event_received

        self.current_tab = "dash"
        self.tab_buttons = {}
        self.tab_frames = {}

        self.setup_styles()
        self.init_ui()

        # Preload default profile configuration into all studio tables
        self.load_default_config_to_ui()

        # Auto-start engine in background thread on launch
        self.on_start_engine()

        # Start periodic polling (every 250ms)
        self.root.after(250, self.poll_state)

    def setup_styles(self):
        self.style = ttk.Style()
        self.style.theme_use("clam")

        bg_dark = "#0b0f19"
        card_bg = "#131c31"
        accent_blue = "#2563eb"
        text_white = "#f8fafc"
        text_muted = "#94a3b8"

        self.style.configure(".", background=bg_dark, foreground=text_white, font=("Helvetica", 10))
        self.style.configure("Card.TFrame", background=card_bg, relief="flat", borderwidth=1)
        self.style.configure("Primary.TButton", background=accent_blue, foreground="#ffffff", font=("Helvetica", 10, "bold"), padding=[10, 5])
        self.style.map("Primary.TButton", background=[("active", "#1d4ed8")])

        self.style.configure("Success.TButton", background="#059669", foreground="#ffffff", font=("Helvetica", 10, "bold"), padding=[10, 5])
        self.style.map("Success.TButton", background=[("active", "#047857")])

        self.style.configure("Danger.TButton", background="#dc2626", foreground="#ffffff", font=("Helvetica", 10, "bold"), padding=[10, 5])
        self.style.map("Danger.TButton", background=[("active", "#b91c1c")])

        self.style.configure("Treeview", background="#0f172a", foreground="#f8fafc", fieldbackground="#0f172a", rowheight=25)
        self.style.configure("Treeview.Heading", background=card_bg, foreground=text_muted, font=("Helvetica", 9, "bold"))
        self.style.map("Treeview", background=[("selected", "#1e3a8a")], foreground=[("selected", "#ffffff")])

        self.style.configure("TProgressbar", thickness=16, troughcolor="#0f172a", background="#10b981")

    def init_ui(self):
        # ==========================================
        # 1. Top Header (Brand & Global Status)
        # ==========================================
        top_header = tk.Frame(self.root, bg="#0f172a", height=52)
        top_header.pack(fill="x", side="top")

        brand_frame = tk.Frame(top_header, bg="#0f172a")
        brand_frame.pack(side="left", padx=16, pady=8)

        lbl_logo = tk.Label(brand_frame, text="TIBIABOT", font=("Helvetica", 14, "bold"), bg="#0f172a", fg="#38bdf8")
        lbl_logo.pack(side="left")
        lbl_edition = tk.Label(brand_frame, text="  //  SHROUDED EDITION", font=("Helvetica", 9, "bold"), bg="#0f172a", fg="#64748b")
        lbl_edition.pack(side="left", pady=(3, 0))

        right_header = tk.Frame(top_header, bg="#0f172a")
        right_header.pack(side="right", padx=16)

        self.lbl_header_ipc = tk.Label(right_header, text=f"IPC: 127.0.0.1:{self.port}", font=("Helvetica", 9), bg="#0f172a", fg="#94a3b8")
        self.lbl_header_ipc.pack(side="left", padx=10)

        self.lbl_header_status = tk.Label(right_header, text="● Engine: Starting...", font=("Helvetica", 10, "bold"), bg="#0f172a", fg="#f59e0b")
        self.lbl_header_status.pack(side="left", padx=6)

        # ==========================================
        # 2. Main Top Tab Bar (Zakładki wzorowane na Shrouded Bot)
        # ==========================================
        tab_bar_frame = tk.Frame(self.root, bg="#0f172a", height=44)
        tab_bar_frame.pack(fill="x", side="top")

        # Tab definitions: id, icon + title
        tabs = [
            ("dash",     "📊 Dashboard & HUD"),
            ("heal",     "💊 Healing"),
            ("cave",     "🗺️ Cavebot"),
            ("targ",     "⚔️ Targeting"),
            ("loot",     "🎒 Looting"),
            ("aoe",      "🔥 Smart AOE"),
            ("scripts",  "📜 Lua Scripts"),
            ("settings", "⚙️ Settings")
        ]

        tabs_container = tk.Frame(tab_bar_frame, bg="#0f172a")
        tabs_container.pack(side="left", padx=12, pady=4)

        for tab_id, tab_label in tabs:
            btn = tk.Label(
                tabs_container,
                text=tab_label,
                font=("Helvetica", 10, "bold"),
                bg="#1e293b",
                fg="#94a3b8",
                padx=14,
                pady=7,
                cursor="hand2",
                relief="flat"
            )
            btn.pack(side="left", padx=3)
            btn.bind("<Button-1>", lambda e, tid=tab_id: self.switch_tab(tid))

            # Hover effects
            def make_hover(widget, tid):
                widget.bind("<Enter>", lambda e: widget.configure(bg="#334155", fg="#ffffff") if self.current_tab != tid else None)
                widget.bind("<Leave>", lambda e: widget.configure(bg="#1e293b", fg="#94a3b8") if self.current_tab != tid else None)
            make_hover(btn, tab_id)

            self.tab_buttons[tab_id] = btn

        # ==========================================
        # 3. Main Content Container
        # ==========================================
        self.content_container = tk.Frame(self.root, bg="#0b0f19")
        self.content_container.pack(fill="both", expand=True)

        # Create child frame for each tab
        for tab_id, _ in tabs:
            self.tab_frames[tab_id] = tk.Frame(self.content_container, bg="#0b0f19")

        # Build each tab's UI
        self.build_dashboard_tab(self.tab_frames["dash"])
        self.build_healing_tab(self.tab_frames["heal"])
        self.build_cavebot_tab(self.tab_frames["cave"])
        self.build_targeting_tab(self.tab_frames["targ"])
        self.build_looting_tab(self.tab_frames["loot"])
        self.build_smart_aoe_tab(self.tab_frames["aoe"])
        self.build_scripts_tab(self.tab_frames["scripts"])
        self.build_settings_tab(self.tab_frames["settings"])

        # Default active tab: Dashboard
        self.switch_tab("dash")

    def switch_tab(self, tab_id):
        """Switches active view and updates top tab styles."""
        self.current_tab = tab_id

        # Show selected frame, hide all others
        for tid, frame in self.tab_frames.items():
            if tid == tab_id:
                frame.pack(fill="both", expand=True)
            else:
                frame.pack_forget()

        # Update tab button highlights
        for tid, btn in self.tab_buttons.items():
            if tid == tab_id:
                btn.configure(bg="#2563eb", fg="#ffffff")
            else:
                btn.configure(bg="#1e293b", fg="#94a3b8")

    def load_default_config_to_ui(self):
        """Loads configs/default.json into tables on startup."""
        default_file = os.path.join(self.configs_dir, "default.json")
        if not os.path.exists(default_file):
            return

        try:
            with open(default_file, "r", encoding="utf-8") as f:
                cfg = json.load(f)

            # 1. Healing
            heal_rules = cfg.get("Healing", {}).get("rules", [])
            for r in heal_rules:
                val = r.get("spellWords") if r.get("action") == "spell" else str(r.get("itemId"))
                self.tree_heal.insert("", "end", values=(
                    r.get("name"), r.get("trigger"), f"{r.get('thresholdPercent')}%",
                    r.get("action"), val, r.get("minMana"), f"{r.get('cooldownMs')}ms", r.get("priority")
                ))

            # 2. Cavebot
            cave_wps = cfg.get("Cavebot", {}).get("waypoints", [])
            for i, wp in enumerate(cave_wps):
                extra = wp.get("script") if wp.get("type") == "action" else f"Tol: {wp.get('tolerance', 1)}"
                self.tree_cave.insert("", "end", values=(
                    i + 1, wp.get("type").upper(), wp.get("x"), wp.get("y"), wp.get("z"), wp.get("label"), extra
                ))

            # 3. Targeting
            targ_rules = cfg.get("Targeting", {}).get("targets", [])
            for t in targ_rules:
                self.tree_targ.insert("", "end", values=(
                    t.get("name"), t.get("priority"), t.get("stance").upper(), t.get("desiredDistance"), ", ".join(t.get("spells", []))
                ))

            # 4. Looting
            loot_items = cfg.get("Looting", {}).get("items", [])
            for it in loot_items:
                self.tree_loot.insert("", "end", values=(it.get("id"), it.get("name"), f"Backpack #{it.get('destContainer')}"))

            # 5. Smart AOE
            aoe_rules = cfg.get("SmartAOE", {}).get("rules", [])
            for r in aoe_rules:
                self.tree_aoe.insert("", "end", values=(
                    r.get("spell"), r.get("minMonsters"), f"{r.get('maxDistance')} tiles", r.get("minMana"), f"{r.get('cooldownMs')}ms"
                ))

        except Exception as e:
            print("Error loading default config:", e)

    # ==========================================
    # 1. Dashboard Tab
    # ==========================================
    def build_dashboard_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=14)

        # Top Control Row
        ctrl_card = ttk.Frame(container, style="Card.TFrame", padding=12)
        ctrl_card.pack(fill="x", pady=(0, 10))

        c_left = tk.Frame(ctrl_card, bg="#131c31")
        c_left.pack(side="left")
        ttk.Button(c_left, text="▶ Start Engine", style="Success.TButton", command=self.on_start_engine).pack(side="left", padx=4)
        ttk.Button(c_left, text="⏸ Pause", command=self.on_pause_engine).pack(side="left", padx=4)
        ttk.Button(c_left, text="⏹ Stop Engine", style="Danger.TButton", command=self.on_stop_engine).pack(side="left", padx=4)

        c_right = tk.Frame(ctrl_card, bg="#131c31")
        c_right.pack(side="right")
        ttk.Button(c_right, text="🧪 Quick Heal (Vita)", command=lambda: self.quick_action("heal")).pack(side="left", padx=3)
        ttk.Button(c_right, text="⚡ Quick Haste", command=lambda: self.quick_action("haste")).pack(side="left", padx=3)
        ttk.Button(c_right, text="🧹 Clear Logs", command=self.clear_logs).pack(side="left", padx=3)

        # Mid Row: HUD Card (Left) + Battle List Card (Right)
        mid_row = tk.Frame(container, bg="#0b0f19")
        mid_row.pack(fill="x", pady=(0, 10))

        # HUD Card
        hud_card = ttk.Frame(mid_row, style="Card.TFrame", padding=12)
        hud_card.pack(side="left", fill="both", expand=True, padx=(0, 6))

        tk.Label(hud_card, text="CHARACTER HUD & VITALS", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w")
        self.lbl_player_info = tk.Label(hud_card, text="Name: MockPlayer   |   Level: 100   |   Pos: (32300, 32200, 7)   |   Cap: 450.0", bg="#131c31", fg="#38bdf8", font=("Helvetica", 10, "bold"))
        self.lbl_player_info.pack(anchor="w", pady=(4, 2))

        self.lbl_hp = tk.Label(hud_card, text="Health: 1000 / 1000 (100%)", bg="#131c31", fg="#f87171", font=("Helvetica", 9, "bold"))
        self.lbl_hp.pack(anchor="w")
        self.bar_hp = ttk.Progressbar(hud_card, style="TProgressbar", maximum=100, value=100)
        self.bar_hp.pack(fill="x", pady=(1, 4))

        self.lbl_mp = tk.Label(hud_card, text="Mana: 500 / 500 (100%)", bg="#131c31", fg="#60a5fa", font=("Helvetica", 9, "bold"))
        self.lbl_mp.pack(anchor="w")
        self.bar_mp = ttk.Progressbar(hud_card, style="TProgressbar", maximum=100, value=100)
        self.bar_mp.pack(fill="x", pady=(1, 4))

        # Battle List Card
        battle_card = ttk.Frame(mid_row, style="Card.TFrame", padding=12)
        battle_card.pack(side="right", fill="both", expand=True, padx=(6, 0))

        tk.Label(battle_card, text="BATTLE LIST & DETECTED ENTITIES", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w")

        cols = ("id", "name", "hp", "dist")
        self.tree_battle = ttk.Treeview(battle_card, columns=cols, show="headings", height=4)
        self.tree_battle.heading("id", text="ID")
        self.tree_battle.heading("name", text="Monster")
        self.tree_battle.heading("hp", text="HP %")
        self.tree_battle.heading("dist", text="Dist")
        self.tree_battle.column("id", width=60)
        self.tree_battle.column("name", width=110)
        self.tree_battle.column("hp", width=60)
        self.tree_battle.column("dist", width=50)
        self.tree_battle.pack(fill="both", expand=True, pady=(4, 0))

        # Modules Hub Card (Każdy moduł z bezpośrednim przyciskiem do swojej zakładki)
        mods_card = ttk.Frame(container, style="Card.TFrame", padding=12)
        mods_card.pack(fill="x", pady=(0, 10))

        tk.Label(mods_card, text="MODULES OVERVIEW & DIRECT TAB ACCESS", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 4))

        self.mod_vars = {}
        module_rows = [
            ("Healing",   "heal", "Rules: Emergency Vita (<50%), Gran (<75%), Health Pot (<30%)", "#10b981"),
            ("Cavebot",   "cave", "Waypoints: 2 Points Loaded (Start -> Spawn Area)",              "#3b82f6"),
            ("Targeting", "targ", "Targets: Dragon (Priority 1, Chase, exori hur)",               "#f59e0b"),
            ("Looting",   "loot", "Items: Gold Coin (3031), Platinum Coin (3035)",                "#8b5cf6"),
            ("SmartAOE",  "aoe",  "AOE: exori (>= 3 Monsters, Max Dist 1 tile)",                  "#ec4899")
        ]

        for name, tab_target, summary_txt, color in module_rows:
            row = tk.Frame(mods_card, bg="#131c31")
            row.pack(fill="x", pady=2)

            var = tk.BooleanVar(value=True if name in ["Healing", "Cavebot", "Targeting"] else False)
            chk = tk.Checkbutton(
                row,
                text=f"{name}",
                variable=var,
                bg="#131c31",
                fg="#f8fafc",
                selectcolor="#0f172a",
                activebackground="#131c31",
                activeforeground="#ffffff",
                font=("Helvetica", 10, "bold"),
                command=lambda n=name, v=var: self.on_mod_check(n, v.get())
            )
            chk.pack(side="left")
            self.mod_vars[name] = var

            lbl_sum = tk.Label(row, text=f"  —  {summary_txt}", font=("Helvetica", 9), bg="#131c31", fg="#94a3b8")
            lbl_sum.pack(side="left", padx=4)

            btn_open = tk.Label(
                row,
                text=f" ➔ Otwórz zakładkę {name} ",
                bg="#1e293b",
                fg="#38bdf8",
                padx=10,
                pady=2,
                font=("Helvetica", 9, "bold"),
                cursor="hand2"
            )
            btn_open.pack(side="right")
            btn_open.bind("<Button-1>", lambda e, t=tab_target: self.switch_tab(t))
            btn_open.bind("<Enter>", lambda e, w=btn_open: w.configure(bg="#2563eb", fg="#ffffff"))
            btn_open.bind("<Leave>", lambda e, w=btn_open: w.configure(bg="#1e293b", fg="#38bdf8"))

        # Live Logs Console
        log_card = ttk.Frame(container, style="Card.TFrame", padding=10)
        log_card.pack(fill="both", expand=True)

        tk.Label(log_card, text="REAL-TIME ENGINE TELEMETRY & EVENT STREAM", font=("Helvetica", 9, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w")
        self.txt_log = scrolledtext.ScrolledText(log_card, bg="#090d16", fg="#38bdf8", insertbackground="white", font=("Courier New", 9), height=7)
        self.txt_log.pack(fill="both", expand=True, pady=4)

    # ==========================================
    # 2. Healing Studio Tab
    # ==========================================
    def build_healing_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="💊 HEALING STUDIO // RULES & PRIORITIES", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="Fetch Config", command=self.fetch_healing).pack(side="right", padx=4)
        ttk.Button(header, text="Apply to Engine", style="Primary.TButton", command=self.apply_healing).pack(side="right", padx=4)

        cols = ("name", "trigger", "threshold", "action", "value", "min_mana", "cooldown", "priority")
        self.tree_heal = ttk.Treeview(container, columns=cols, show="headings", height=9)
        self.tree_heal.heading("name", text="Rule Name")
        self.tree_heal.heading("trigger", text="Trigger Condition")
        self.tree_heal.heading("threshold", text="Threshold")
        self.tree_heal.heading("action", text="Action")
        self.tree_heal.heading("value", text="Spell Words / Item ID")
        self.tree_heal.heading("min_mana", text="Min Mana")
        self.tree_heal.heading("cooldown", text="Cooldown")
        self.tree_heal.heading("priority", text="Priority")
        self.tree_heal.pack(fill="both", expand=True, pady=(0, 10))

        form = ttk.Frame(container, style="Card.TFrame", padding=12)
        form.pack(fill="x")

        tk.Label(form, text="Create Healing Rule", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 6))

        r1 = tk.Frame(form, bg="#131c31")
        r1.pack(fill="x", pady=3)

        tk.Label(r1, text="Name:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.in_heal_name = tk.Entry(r1, width=12, bg="#0f172a", fg="white")
        self.in_heal_name.insert(0, "Vita")
        self.in_heal_name.pack(side="left", padx=4)

        tk.Label(r1, text="Trigger:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.cb_heal_trig = ttk.Combobox(r1, values=["hp_below", "mp_below", "poison"], width=10)
        self.cb_heal_trig.set("hp_below")
        self.cb_heal_trig.pack(side="left", padx=4)

        tk.Label(r1, text="Threshold %:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_heal_thresh = tk.Entry(r1, width=6, bg="#0f172a", fg="white")
        self.in_heal_thresh.insert(0, "50")
        self.in_heal_thresh.pack(side="left", padx=4)

        tk.Label(r1, text="Action:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.cb_heal_act = ttk.Combobox(r1, values=["spell", "item"], width=8)
        self.cb_heal_act.set("spell")
        self.cb_heal_act.pack(side="left", padx=4)

        tk.Label(r1, text="Spell/ID:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_heal_val = tk.Entry(r1, width=14, bg="#0f172a", fg="white")
        self.in_heal_val.insert(0, "exura vita")
        self.in_heal_val.pack(side="left", padx=4)

        r2 = tk.Frame(form, bg="#131c31")
        r2.pack(fill="x", pady=4)
        tk.Label(r2, text="Min Mana:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.in_heal_mana = tk.Entry(r2, width=6, bg="#0f172a", fg="white")
        self.in_heal_mana.insert(0, "160")
        self.in_heal_mana.pack(side="left", padx=4)

        tk.Label(r2, text="Cooldown (ms):", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_heal_cd = tk.Entry(r2, width=6, bg="#0f172a", fg="white")
        self.in_heal_cd.insert(0, "1000")
        self.in_heal_cd.pack(side="left", padx=4)

        tk.Label(r2, text="Priority:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_heal_prio = tk.Entry(r2, width=4, bg="#0f172a", fg="white")
        self.in_heal_prio.insert(0, "1")
        self.in_heal_prio.pack(side="left", padx=4)

        ttk.Button(r2, text="Add Rule", style="Success.TButton", command=self.add_healing_rule).pack(side="right", padx=4)
        ttk.Button(r2, text="Delete Selected", style="Danger.TButton", command=self.delete_healing_rule).pack(side="right", padx=4)

    # ==========================================
    # 3. Cavebot Tab
    # ==========================================
    def build_cavebot_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="🗺️ CAVEBOT // WAYPOINTS & NAVIGATION", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="Fetch Config", command=self.fetch_cavebot).pack(side="right", padx=4)
        ttk.Button(header, text="Apply to Engine", style="Primary.TButton", command=self.apply_cavebot).pack(side="right", padx=4)

        cols = ("num", "type", "x", "y", "z", "label", "extra")
        self.tree_cave = ttk.Treeview(container, columns=cols, show="headings", height=9)
        self.tree_cave.heading("num", text="#")
        self.tree_cave.heading("type", text="Type")
        self.tree_cave.heading("x", text="X")
        self.tree_cave.heading("y", text="Y")
        self.tree_cave.heading("z", text="Z")
        self.tree_cave.heading("label", text="Label")
        self.tree_cave.heading("extra", text="Details")
        self.tree_cave.column("num", width=40)
        self.tree_cave.column("type", width=80)
        self.tree_cave.column("x", width=70)
        self.tree_cave.column("y", width=70)
        self.tree_cave.column("z", width=40)
        self.tree_cave.pack(fill="both", expand=True, pady=(0, 10))

        form = ttk.Frame(container, style="Card.TFrame", padding=12)
        form.pack(fill="x")

        tk.Label(form, text="Add New Waypoint", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 6))

        r = tk.Frame(form, bg="#131c31")
        r.pack(fill="x", pady=2)

        tk.Label(r, text="Type:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.cb_wp_type = ttk.Combobox(r, values=["walk", "node", "stand", "action", "rope", "ladder"], width=8)
        self.cb_wp_type.set("walk")
        self.cb_wp_type.pack(side="left", padx=4)

        tk.Label(r, text="X:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_wp_x = tk.Entry(r, width=7, bg="#0f172a", fg="white")
        self.in_wp_x.insert(0, "32300")
        self.in_wp_x.pack(side="left", padx=2)

        tk.Label(r, text="Y:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_wp_y = tk.Entry(r, width=7, bg="#0f172a", fg="white")
        self.in_wp_y.insert(0, "32200")
        self.in_wp_y.pack(side="left", padx=2)

        tk.Label(r, text="Z:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_wp_z = tk.Entry(r, width=4, bg="#0f172a", fg="white")
        self.in_wp_z.insert(0, "7")
        self.in_wp_z.pack(side="left", padx=2)

        tk.Label(r, text="Label:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_wp_lbl = tk.Entry(r, width=10, bg="#0f172a", fg="white")
        self.in_wp_lbl.insert(0, "Spot 1")
        self.in_wp_lbl.pack(side="left", padx=2)

        ttk.Button(r, text="📍 Current Pos", command=self.add_current_pos_wp).pack(side="left", padx=6)
        ttk.Button(r, text="Add WP", style="Success.TButton", command=self.add_cavebot_wp).pack(side="right", padx=4)
        ttk.Button(r, text="Delete Selected", style="Danger.TButton", command=self.delete_cavebot_wp).pack(side="right", padx=4)

    # ==========================================
    # 4. Targeting Tab
    # ==========================================
    def build_targeting_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="⚔️ TARGETING & COMBAT CONFIGURATION", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="Fetch Config", command=self.fetch_targeting).pack(side="right", padx=4)
        ttk.Button(header, text="Apply to Engine", style="Primary.TButton", command=self.apply_targeting).pack(side="right", padx=4)

        cols = ("name", "priority", "stance", "dist", "spells")
        self.tree_targ = ttk.Treeview(container, columns=cols, show="headings", height=9)
        self.tree_targ.heading("name", text="Target Creature (* = all)")
        self.tree_targ.heading("priority", text="Priority")
        self.tree_targ.heading("stance", text="Combat Stance")
        self.tree_targ.heading("dist", text="Desired Dist")
        self.tree_targ.heading("spells", text="Attack Spells")
        self.tree_targ.pack(fill="both", expand=True, pady=(0, 10))

        form = ttk.Frame(container, style="Card.TFrame", padding=12)
        form.pack(fill="x")

        tk.Label(form, text="Add Target Rule", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 6))

        r = tk.Frame(form, bg="#131c31")
        r.pack(fill="x", pady=2)

        tk.Label(r, text="Creature:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.in_targ_name = tk.Entry(r, width=14, bg="#0f172a", fg="white")
        self.in_targ_name.insert(0, "Dragon Lord")
        self.in_targ_name.pack(side="left", padx=4)

        tk.Label(r, text="Priority:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_targ_prio = tk.Entry(r, width=4, bg="#0f172a", fg="white")
        self.in_targ_prio.insert(0, "1")
        self.in_targ_prio.pack(side="left", padx=4)

        tk.Label(r, text="Stance:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.cb_targ_stance = ttk.Combobox(r, values=["chase", "distance", "stand"], width=9)
        self.cb_targ_stance.set("chase")
        self.cb_targ_stance.pack(side="left", padx=4)

        tk.Label(r, text="Spells (comma-separated):", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_targ_spells = tk.Entry(r, width=16, bg="#0f172a", fg="white")
        self.in_targ_spells.insert(0, "exori hur")
        self.in_targ_spells.pack(side="left", padx=4)

        ttk.Button(r, text="Add Target", style="Success.TButton", command=self.add_targeting_rule).pack(side="right", padx=4)
        ttk.Button(r, text="Delete Selected", style="Danger.TButton", command=self.delete_targeting_rule).pack(side="right", padx=4)

    # ==========================================
    # 5. Looting Tab
    # ==========================================
    def build_looting_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="🎒 LOOTING & ITEM COLLECTION", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="Fetch Config", command=self.fetch_looting).pack(side="right", padx=4)
        ttk.Button(header, text="Apply to Engine", style="Primary.TButton", command=self.apply_looting).pack(side="right", padx=4)

        cols = ("id", "name", "dest")
        self.tree_loot = ttk.Treeview(container, columns=cols, show="headings", height=9)
        self.tree_loot.heading("id", text="Item ID")
        self.tree_loot.heading("name", text="Item Name")
        self.tree_loot.heading("dest", text="Destination Container")
        self.tree_loot.column("id", width=100)
        self.tree_loot.column("name", width=200)
        self.tree_loot.pack(fill="both", expand=True, pady=(0, 10))

        form = ttk.Frame(container, style="Card.TFrame", padding=12)
        form.pack(fill="x")

        tk.Label(form, text="Add Item to Loot List", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 6))

        r = tk.Frame(form, bg="#131c31")
        r.pack(fill="x", pady=2)

        tk.Label(r, text="Item ID:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.in_loot_id = tk.Entry(r, width=8, bg="#0f172a", fg="white")
        self.in_loot_id.insert(0, "3035")
        self.in_loot_id.pack(side="left", padx=4)

        tk.Label(r, text="Name:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_loot_name = tk.Entry(r, width=16, bg="#0f172a", fg="white")
        self.in_loot_name.insert(0, "Platinum Coin")
        self.in_loot_name.pack(side="left", padx=4)

        tk.Label(r, text="Container #:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_loot_dest = tk.Entry(r, width=4, bg="#0f172a", fg="white")
        self.in_loot_dest.insert(0, "0")
        self.in_loot_dest.pack(side="left", padx=4)

        ttk.Button(r, text="+ Gold", command=lambda: self.quick_add_loot(3031, "Gold Coin")).pack(side="left", padx=4)
        ttk.Button(r, text="+ Plat", command=lambda: self.quick_add_loot(3035, "Platinum Coin")).pack(side="left", padx=4)
        ttk.Button(r, text="+ Crystal", command=lambda: self.quick_add_loot(3043, "Crystal Coin")).pack(side="left", padx=4)

        ttk.Button(r, text="Add Item", style="Success.TButton", command=self.add_looting_item).pack(side="right", padx=4)
        ttk.Button(r, text="Delete Selected", style="Danger.TButton", command=self.delete_looting_item).pack(side="right", padx=4)

    # ==========================================
    # 6. Smart AOE Tab
    # ==========================================
    def build_smart_aoe_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="🔥 SMART AOE // DYNAMIC SPELL ROTATION", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="Fetch Config", command=self.fetch_aoe).pack(side="right", padx=4)
        ttk.Button(header, text="Apply to Engine", style="Primary.TButton", command=self.apply_aoe).pack(side="right", padx=4)

        cols = ("spell", "min_mobs", "dist", "mana", "cd")
        self.tree_aoe = ttk.Treeview(container, columns=cols, show="headings", height=9)
        self.tree_aoe.heading("spell", text="AOE Spell Words")
        self.tree_aoe.heading("min_mobs", text="Min Monsters")
        self.tree_aoe.heading("dist", text="Max Distance")
        self.tree_aoe.heading("mana", text="Min Mana")
        self.tree_aoe.heading("cd", text="Cooldown")
        self.tree_aoe.pack(fill="both", expand=True, pady=(0, 10))

        form = ttk.Frame(container, style="Card.TFrame", padding=12)
        form.pack(fill="x")

        tk.Label(form, text="Create AOE Rule", font=("Helvetica", 10, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 6))

        r = tk.Frame(form, bg="#131c31")
        r.pack(fill="x", pady=2)

        tk.Label(r, text="Spell:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.in_aoe_spell = tk.Entry(r, width=14, bg="#0f172a", fg="white")
        self.in_aoe_spell.insert(0, "exori gran")
        self.in_aoe_spell.pack(side="left", padx=4)

        tk.Label(r, text="Min Mobs:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_aoe_mobs = tk.Entry(r, width=4, bg="#0f172a", fg="white")
        self.in_aoe_mobs.insert(0, "3")
        self.in_aoe_mobs.pack(side="left", padx=4)

        tk.Label(r, text="Max Dist:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_aoe_dist = tk.Entry(r, width=4, bg="#0f172a", fg="white")
        self.in_aoe_dist.insert(0, "1")
        self.in_aoe_dist.pack(side="left", padx=4)

        tk.Label(r, text="Min Mana:", bg="#131c31", fg="#94a3b8").pack(side="left", padx=4)
        self.in_aoe_mana = tk.Entry(r, width=6, bg="#0f172a", fg="white")
        self.in_aoe_mana.insert(0, "340")
        self.in_aoe_mana.pack(side="left", padx=4)

        ttk.Button(r, text="Add AOE Rule", style="Success.TButton", command=self.add_aoe_rule).pack(side="right", padx=4)
        ttk.Button(r, text="Delete Selected", style="Danger.TButton", command=self.delete_aoe_rule).pack(side="right", padx=4)

    # ==========================================
    # 7. Lua Scripts Studio Tab
    # ==========================================
    def build_scripts_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        header = tk.Frame(container, bg="#0b0f19")
        header.pack(fill="x", pady=(0, 10))

        tk.Label(header, text="📜 LUA SCRIPT STUDIO // REAL-TIME EXECUTION", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(side="left")
        ttk.Button(header, text="▶ Run in Engine", style="Success.TButton", command=self.run_lua_script).pack(side="right", padx=4)
        ttk.Button(header, text="💾 Save Script", command=self.save_lua_script).pack(side="right", padx=4)
        ttk.Button(header, text="🔄 Reload Files", command=self.load_script_files).pack(side="right", padx=4)

        paned = tk.PanedWindow(container, orient="horizontal", bg="#0b0f19", sashrelief="flat")
        paned.pack(fill="both", expand=True)

        left_p = ttk.Frame(paned, style="Card.TFrame", padding=8)
        paned.add(left_p, minsize=200)

        tk.Label(left_p, text="Installed Scripts", font=("Helvetica", 9, "bold"), bg="#131c31", fg="#94a3b8").pack(anchor="w")
        self.list_scripts = tk.Listbox(left_p, bg="#0f172a", fg="#f8fafc", selectbackground="#2563eb", borderwidth=0)
        self.list_scripts.pack(fill="both", expand=True, pady=4)
        self.list_scripts.bind("<<ListboxSelect>>", self.on_script_select)

        right_p = ttk.Frame(paned, style="Card.TFrame", padding=8)
        paned.add(right_p, minsize=400)

        tk.Label(right_p, text="Script Editor (Lua 5.4 / C++ Bindings: Game.*, Bot.*)", font=("Helvetica", 9, "bold"), bg="#131c31", fg="#94a3b8").pack(anchor="w")
        self.txt_script = scrolledtext.ScrolledText(right_p, bg="#090d16", fg="#e2e8f0", insertbackground="white", font=("Courier New", 10))
        self.txt_script.pack(fill="both", expand=True, pady=4)

        default_lua = '-- Example Lua Script\nlocal player = Game.getPlayer()\nBot.log("Player HP: " .. player.health .. " / " .. player.maxHealth)\nif player.health < 600 then\n    Bot.castSpell("exura vita")\nend\n'
        self.txt_script.insert("1.0", default_lua)
        self.load_script_files()

    # ==========================================
    # 8. Settings Tab
    # ==========================================
    def build_settings_tab(self, parent):
        container = tk.Frame(parent, bg="#0b0f19")
        container.pack(fill="both", expand=True, padx=16, pady=16)

        tk.Label(container, text="⚙️ PROFILES & GLOBAL BOT SETTINGS", font=("Helvetica", 13, "bold"), bg="#0b0f19", fg="#38bdf8").pack(anchor="w", pady=(0, 12))

        # Profile Card
        card1 = ttk.Frame(container, style="Card.TFrame", padding=16)
        card1.pack(fill="x", pady=6)
        tk.Label(card1, text="Profile Management", font=("Helvetica", 11, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 8))

        p_row = tk.Frame(card1, bg="#131c31")
        p_row.pack(fill="x", pady=4)
        tk.Label(p_row, text="Active Profile:", bg="#131c31", fg="#94a3b8").pack(side="left")
        self.cb_profile = ttk.Combobox(p_row, values=["default", "knight_hunt", "mage_sd", "paladin_aoe"], width=16)
        self.cb_profile.set("default")
        self.cb_profile.pack(side="left", padx=8)

        ttk.Button(p_row, text="Load Profile", command=self.load_profile_btn).pack(side="left", padx=4)
        ttk.Button(p_row, text="Save Profile", style="Primary.TButton", command=self.save_profile_btn).pack(side="left", padx=4)

        # Connection Card
        card2 = ttk.Frame(container, style="Card.TFrame", padding=16)
        card2.pack(fill="x", pady=6)
        tk.Label(card2, text="Engine IPC & Architecture", font=("Helvetica", 11, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 8))

        c_row = tk.Frame(card2, bg="#131c31")
        c_row.pack(fill="x", pady=4)
        tk.Label(c_row, text=f"Protocol: JSON-RPC 2.0 over TCP   |   Host: 127.0.0.1   |   Port: {self.port}   |   Core: C++20", bg="#131c31", fg="#94a3b8").pack(side="left")

        # PySide6 Switcher Card
        card3 = ttk.Frame(container, style="Card.TFrame", padding=16)
        card3.pack(fill="x", pady=6)
        tk.Label(card3, text="Modern PySide6 (Qt6) Launcher", font=("Helvetica", 11, "bold"), bg="#131c31", fg="#f1f5f9").pack(anchor="w", pady=(0, 4))
        tk.Label(card3, text="A full Qt6 PySide6 launcher is available in .venv with complete hardware-accelerated animations and styling.", bg="#131c31", fg="#94a3b8").pack(anchor="w", pady=(0, 8))
        ttk.Button(card3, text="🚀 Launch PySide6 / Qt6 Launcher", style="Primary.TButton", command=self.launch_pyside6).pack(anchor="w")

    # ==========================================
    # Logic & Event Handlers
    # ==========================================
    def on_log_received(self, level, message, timestamp):
        def _append():
            color = "#38bdf8"
            if level == "Warning":
                color = "#f59e0b"
            elif level == "Error":
                color = "#ef4444"
            self.txt_log.insert(tk.END, f"{timestamp} [{level}] {message}\n")
            self.txt_log.see(tk.END)
        self.root.after(0, _append)

    def on_event_received(self, event_type, data):
        def _log():
            self.txt_log.insert(tk.END, f"[EVENT] {event_type}: {json.dumps(data)}\n")
            self.txt_log.see(tk.END)
        self.root.after(0, _log)

    def on_start_engine(self):
        def _do_start():
            if not self.engine_proc.is_running():
                ok = self.engine_proc.start()
                if ok:
                    time.sleep(0.4)
                    self.client.connect()
            else:
                self.client.resume_engine()
        threading.Thread(target=_do_start, daemon=True).start()

    def on_pause_engine(self):
        threading.Thread(target=self.client.pause_engine, daemon=True).start()

    def on_stop_engine(self):
        def _do_stop():
            self.client.stop_engine()
            self.engine_proc.stop()
            self.client.disconnect()
        threading.Thread(target=_do_stop, daemon=True).start()

    def quick_action(self, action):
        def _do():
            if action == "heal":
                self.client.execute_lua('Bot.castSpell("exura vita")')
            elif action == "haste":
                self.client.execute_lua('Bot.castSpell("utani hur")')
        threading.Thread(target=_do, daemon=True).start()

    def clear_logs(self):
        self.txt_log.delete("1.0", tk.END)

    def on_mod_check(self, name, enabled):
        threading.Thread(target=lambda: self.client.set_module_enabled(name, enabled), daemon=True).start()

    def poll_state(self):
        def _fetch():
            if self.engine_proc.is_running() and not self.client.is_connected:
                self.client.connect()

            if self.client.is_connected:
                state = self.client.get_game_state()
                self.root.after(0, lambda: self.update_telemetry(state))
            else:
                self.root.after(0, self.update_offline_status)

        threading.Thread(target=_fetch, daemon=True).start()
        self.root.after(250, self.poll_state)

    def update_telemetry(self, state):
        if not state:
            return

        self.lbl_header_status.configure(text="● Engine: Running", fg="#10b981")
        self.lbl_header_ipc.configure(text=f"IPC: Connected (Port {self.port})", fg="#38bdf8")

        p = state.get("player", {})
        hp = p.get("health", 1000)
        max_hp = p.get("maxHealth", 1000)
        mp = p.get("mana", 500)
        max_mp = p.get("maxMana", 500)
        name = p.get("name", "MockPlayer")
        lvl = p.get("level", 100)
        pos = p.get("position", {})
        cap = p.get("capacity", 450.0)

        hp_pct = int((hp / max(1, max_hp)) * 100)
        mp_pct = int((mp / max(1, max_mp)) * 100)

        self.lbl_player_info.configure(text=f"Name: {name}   |   Level: {lvl}   |   Pos: ({pos.get('x',0)}, {pos.get('y',0)}, {pos.get('z',0)})   |   Cap: {cap:.1f}")
        self.lbl_hp.configure(text=f"Health: {hp} / {max_hp} ({hp_pct}%)")
        self.bar_hp["value"] = hp_pct

        self.lbl_mp.configure(text=f"Mana: {mp} / {max_mp} ({mp_pct}%)")
        self.bar_mp["value"] = mp_pct

        # Battle List
        monsters = state.get("monsters", [])
        for item in self.tree_battle.get_children():
            self.tree_battle.delete(item)
        for m in monsters:
            m_hp_pct = int((m.get("health", 100) / max(1, m.get("maxHealth", 100))) * 100)
            self.tree_battle.insert("", "end", values=(m.get("id"), m.get("name"), f"{m_hp_pct}%", f"{m.get('distance', 1.0):.1f}"))

    def update_offline_status(self):
        if self.engine_proc.is_running():
            self.lbl_header_status.configure(text="● Engine: Connecting...", fg="#f59e0b")
        else:
            self.lbl_header_status.configure(text="● Engine: Offline", fg="#ef4444")
            self.lbl_header_ipc.configure(text="IPC: Disconnected", fg="#94a3b8")

    # Healing CRUD
    def fetch_healing(self):
        def _fetch():
            cfg = self.client.get_module_config("Healing")
            if cfg and "rules" in cfg:
                def _update():
                    for item in self.tree_heal.get_children():
                        self.tree_heal.delete(item)
                    for r in cfg["rules"]:
                        val = r.get("spellWords") if r.get("action") == "spell" else str(r.get("itemId"))
                        self.tree_heal.insert("", "end", values=(
                            r.get("name"), r.get("trigger"), f"{r.get('thresholdPercent')}%",
                            r.get("action"), val, r.get("minMana"), f"{r.get('cooldownMs')}ms", r.get("priority")
                        ))
                self.root.after(0, _update)
        threading.Thread(target=_fetch, daemon=True).start()

    def apply_healing(self):
        rules = []
        for item in self.tree_heal.get_children():
            vals = self.tree_heal.item(item, "values")
            act = vals[3]
            rule = {
                "name": vals[0],
                "enabled": True,
                "trigger": vals[1],
                "thresholdPercent": float(vals[2].replace("%", "")),
                "action": act,
                "minMana": int(vals[5]),
                "cooldownMs": int(vals[6].replace("ms", "")),
                "priority": int(vals[7])
            }
            if act == "spell":
                rule["spellWords"] = vals[4]
            else:
                rule["itemId"] = int(vals[4])
            rules.append(rule)
        threading.Thread(target=lambda: self.client.set_module_config("Healing", {"rules": rules}), daemon=True).start()
        messagebox.showinfo("Success", "Healing rules applied to Engine!")

    def add_healing_rule(self):
        name = self.in_heal_name.get()
        trig = self.cb_heal_trig.get()
        thresh = self.in_heal_thresh.get()
        act = self.cb_heal_act.get()
        val = self.in_heal_val.get()
        mana = self.in_heal_mana.get()
        cd = self.in_heal_cd.get()
        prio = self.in_heal_prio.get()
        self.tree_heal.insert("", "end", values=(name, trig, f"{thresh}%", act, val, mana, f"{cd}ms", prio))

    def delete_healing_rule(self):
        for sel in self.tree_heal.selection():
            self.tree_heal.delete(sel)

    # Cavebot CRUD
    def fetch_cavebot(self):
        def _fetch():
            cfg = self.client.get_module_config("Cavebot")
            if cfg and "waypoints" in cfg:
                def _update():
                    for item in self.tree_cave.get_children():
                        self.tree_cave.delete(item)
                    for i, wp in enumerate(cfg["waypoints"]):
                        extra = wp.get("script") if wp.get("type") == "action" else f"Tol: {wp.get('tolerance', 1)}"
                        self.tree_cave.insert("", "end", values=(
                            i + 1, wp.get("type").upper(), wp.get("x"), wp.get("y"), wp.get("z"), wp.get("label"), extra
                        ))
                self.root.after(0, _update)
        threading.Thread(target=_fetch, daemon=True).start()

    def apply_cavebot(self):
        wps = []
        for item in self.tree_cave.get_children():
            v = self.tree_cave.item(item, "values")
            wps.append({
                "type": str(v[1]).lower(),
                "x": int(v[2]),
                "y": int(v[3]),
                "z": int(v[4]),
                "label": str(v[5]),
                "script": "",
                "tolerance": 1
            })
        threading.Thread(target=lambda: self.client.set_module_config("Cavebot", {"waypoints": wps}), daemon=True).start()
        messagebox.showinfo("Success", "Cavebot waypoints applied to Engine!")

    def add_cavebot_wp(self):
        idx = len(self.tree_cave.get_children()) + 1
        t = self.cb_wp_type.get().upper()
        x = self.in_wp_x.get()
        y = self.in_wp_y.get()
        z = self.in_wp_z.get()
        lbl = self.in_wp_lbl.get()
        self.tree_cave.insert("", "end", values=(idx, t, x, y, z, lbl, "Tol: 1"))

    def add_current_pos_wp(self):
        state = self.client.get_game_state()
        if state and "player" in state and "position" in state["player"]:
            pos = state["player"]["position"]
            self.in_wp_x.delete(0, tk.END)
            self.in_wp_x.insert(0, str(pos["x"]))
            self.in_wp_y.delete(0, tk.END)
            self.in_wp_y.insert(0, str(pos["y"]))
            self.in_wp_z.delete(0, tk.END)
            self.in_wp_z.insert(0, str(pos["z"]))
            self.add_cavebot_wp()

    def delete_cavebot_wp(self):
        for sel in self.tree_cave.selection():
            self.tree_cave.delete(sel)

    # Targeting CRUD
    def fetch_targeting(self):
        def _fetch():
            cfg = self.client.get_module_config("Targeting")
            if cfg and "targets" in cfg:
                def _update():
                    for item in self.tree_targ.get_children():
                        self.tree_targ.delete(item)
                    for t in cfg["targets"]:
                        self.tree_targ.insert("", "end", values=(
                            t.get("name"), t.get("priority"), t.get("stance").upper(), t.get("desiredDistance"), ", ".join(t.get("spells", []))
                        ))
                self.root.after(0, _update)
        threading.Thread(target=_fetch, daemon=True).start()

    def apply_targeting(self):
        targets = []
        for item in self.tree_targ.get_children():
            v = self.tree_targ.item(item, "values")
            spells = [s.strip() for s in str(v[4]).split(",") if s.strip()]
            targets.append({
                "name": str(v[0]),
                "priority": int(v[1]),
                "stance": str(v[2]).lower(),
                "desiredDistance": int(v[3]),
                "spells": spells
            })
        threading.Thread(target=lambda: self.client.set_module_config("Targeting", {"targets": targets}), daemon=True).start()
        messagebox.showinfo("Success", "Targeting rules applied to Engine!")

    def add_targeting_rule(self):
        name = self.in_targ_name.get()
        prio = self.in_targ_prio.get()
        stance = self.cb_targ_stance.get().upper()
        spells = self.in_targ_spells.get()
        self.tree_targ.insert("", "end", values=(name, prio, stance, "1", spells))

    def delete_targeting_rule(self):
        for sel in self.tree_targ.selection():
            self.tree_targ.delete(sel)

    # Looting CRUD
    def fetch_looting(self):
        def _fetch():
            cfg = self.client.get_module_config("Looting")
            if cfg and "items" in cfg:
                def _update():
                    for item in self.tree_loot.get_children():
                        self.tree_loot.delete(item)
                    for it in cfg["items"]:
                        self.tree_loot.insert("", "end", values=(it.get("id"), it.get("name"), f"Backpack #{it.get('destContainer')}"))
                self.root.after(0, _update)
        threading.Thread(target=_fetch, daemon=True).start()

    def apply_looting(self):
        items = []
        for item in self.tree_loot.get_children():
            v = self.tree_loot.item(item, "values")
            dest = int(str(v[2]).replace("Backpack #", ""))
            items.append({"id": int(v[0]), "name": str(v[1]), "destContainer": dest})
        threading.Thread(target=lambda: self.client.set_module_config("Looting", {"items": items}), daemon=True).start()
        messagebox.showinfo("Success", "Looting items applied to Engine!")

    def add_looting_item(self):
        iid = self.in_loot_id.get()
        name = self.in_loot_name.get()
        dest = self.in_loot_dest.get()
        self.tree_loot.insert("", "end", values=(iid, name, f"Backpack #{dest}"))

    def quick_add_loot(self, iid, name):
        self.tree_loot.insert("", "end", values=(iid, name, "Backpack #0"))

    def delete_looting_item(self):
        for sel in self.tree_loot.selection():
            self.tree_loot.delete(sel)

    # Smart AOE CRUD
    def fetch_aoe(self):
        def _fetch():
            cfg = self.client.get_module_config("SmartAOE")
            if cfg and "rules" in cfg:
                def _update():
                    for item in self.tree_aoe.get_children():
                        self.tree_aoe.delete(item)
                    for r in cfg["rules"]:
                        self.tree_aoe.insert("", "end", values=(
                            r.get("spell"), r.get("minMonsters"), f"{r.get('maxDistance')} tiles", r.get("minMana"), f"{r.get('cooldownMs')}ms"
                        ))
                self.root.after(0, _update)
        threading.Thread(target=_fetch, daemon=True).start()

    def apply_aoe(self):
        rules = []
        for item in self.tree_aoe.get_children():
            v = self.tree_aoe.item(item, "values")
            dist = int(str(v[2]).replace(" tiles", ""))
            cd = int(str(v[4]).replace("ms", ""))
            rules.append({
                "spell": str(v[0]),
                "minMonsters": int(v[1]),
                "maxDistance": dist,
                "minMana": int(v[3]),
                "cooldownMs": cd
            })
        threading.Thread(target=lambda: self.client.set_module_config("SmartAOE", {"rules": rules}), daemon=True).start()
        messagebox.showinfo("Success", "Smart AOE rules applied to Engine!")

    def add_aoe_rule(self):
        spell = self.in_aoe_spell.get()
        mobs = self.in_aoe_mobs.get()
        dist = self.in_aoe_dist.get()
        mana = self.in_aoe_mana.get()
        self.tree_aoe.insert("", "end", values=(spell, mobs, f"{dist} tiles", mana, "1000ms"))

    def delete_aoe_rule(self):
        for sel in self.tree_aoe.selection():
            self.tree_aoe.delete(sel)

    # Lua Scripts CRUD
    def load_script_files(self):
        self.list_scripts.delete(0, tk.END)
        if os.path.exists(self.scripts_dir):
            for f in os.listdir(self.scripts_dir):
                if f.endswith(".lua"):
                    self.list_scripts.insert(tk.END, f)

    def on_script_select(self, event):
        sel = self.list_scripts.curselection()
        if not sel:
            return
        filename = self.list_scripts.get(sel[0])
        path = os.path.join(self.scripts_dir, filename)
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
            self.txt_script.delete("1.0", tk.END)
            self.txt_script.insert("1.0", content)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load script: {e}")

    def save_lua_script(self):
        sel = self.list_scripts.curselection()
        name = self.list_scripts.get(sel[0]) if sel else "script_custom.lua"
        path = os.path.join(self.scripts_dir, name)
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.txt_script.get("1.0", tk.END))
            messagebox.showinfo("Saved", f"Script saved as {name}")
            self.load_script_files()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save script: {e}")

    def run_lua_script(self):
        code = self.txt_script.get("1.0", tk.END).strip()
        if not code:
            return
        threading.Thread(target=lambda: self.client.execute_lua(code), daemon=True).start()

    # Settings & Profiles
    def load_profile_btn(self):
        prof = self.cb_profile.get()
        ok = self.client.load_profile(prof)
        if ok:
            messagebox.showinfo("Success", f"Profile '{prof}' loaded in Engine!")
            self.fetch_healing()
            self.fetch_cavebot()
            self.fetch_targeting()
            self.fetch_looting()
            self.fetch_aoe()
        else:
            messagebox.showwarning("Notice", f"Failed to load profile '{prof}' from Engine.")

    def save_profile_btn(self):
        prof = self.cb_profile.get()
        ok = self.client.save_profile(prof)
        if ok:
            messagebox.showinfo("Success", f"Profile '{prof}' saved!")
        else:
            messagebox.showerror("Error", f"Failed to save profile '{prof}'.")

    def launch_pyside6(self):
        venv_py = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".venv", "bin", "python"))
        main_py = os.path.abspath(os.path.join(os.path.dirname(__file__), "main.py"))
        if os.path.exists(venv_py):
            import subprocess
            subprocess.Popen([venv_py, main_py, "--auto-start"])
            messagebox.showinfo("PySide6", "Launching PySide6 Launcher in a new window...")
        else:
            messagebox.showwarning("PySide6", "Python virtual environment not found.")

def main():
    root = tk.Tk()
    app = TibiaBotTkLauncher(root, port=8765)
    root.mainloop()

if __name__ == "__main__":
    main()
