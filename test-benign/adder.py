import uuid
from datetime import datetime
from pathlib import Path
import shutil
import re


class TaskCoachManager:

    def __init__(self, file_path):
        self.file_path = Path(file_path)

        if not self.file_path.exists():
            raise FileNotFoundError(file_path)

    # --------------------------------------------------
    # Helpers
    # --------------------------------------------------

    def _now(self):
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")

    def _new_id(self):
        return str(uuid.uuid4())

    def _read(self):
        return self.file_path.read_text(encoding="utf-8")

    def _write(self, text):
        self.file_path.write_text(text, encoding="utf-8")

    # --------------------------------------------------
    # Root Insert (after </guid>)
    # --------------------------------------------------

    def _insert_root_task(self, task_block):

        xml = self._read()

        # find </guid>
        guid_close = xml.find("</guid>")
        if guid_close == -1:
            raise Exception("Invalid .tsk structure: missing </guid>")

        insert_pos = guid_close + len("</guid>")

        xml = xml[:insert_pos] + "\n" + task_block + xml[insert_pos:]

        self._write(xml)

    # --------------------------------------------------
    # Insert child before parent closing
    # --------------------------------------------------

    def _insert_child(self, parent_subject, child_block):

        xml = self._read()
        bounds = self._find_task_bounds_by_subject(xml, parent_subject)
        if bounds is None:
            return

        _, close_start, _ = bounds

        xml = xml[:close_start] + "\n" + child_block + xml[close_start:]

        self._write(xml)

    def _find_task_bounds_by_subject(self, xml, subject):
        marker = f'subject="{subject}"'
        subject_pos = xml.find(marker)
        if subject_pos == -1:
            return None

        open_start = xml.rfind("<task", 0, subject_pos)
        if open_start == -1:
            return None

        open_end = xml.find(">", open_start)
        if open_end == -1:
            return None

        open_tag = xml[open_start:open_end + 1]
        if open_tag.rstrip().endswith("/>"):
            return (open_start, open_end + 1, open_end + 1)

        depth = 1
        cursor = open_end + 1

        while depth > 0:
            next_open = xml.find("<task", cursor)
            next_close = xml.find("</task>", cursor)

            if next_close == -1:
                return None

            if next_open != -1 and next_open < next_close:
                next_open_end = xml.find(">", next_open)
                if next_open_end == -1:
                    return None

                nested_open_tag = xml[next_open:next_open_end + 1]
                if not nested_open_tag.rstrip().endswith("/>"):
                    depth += 1

                cursor = next_open_end + 1
                continue

            depth -= 1
            close_start = next_close
            close_end = next_close + len("</task>")
            cursor = close_end

            if depth == 0:
                return (open_start, close_start, close_end)

        return None

    def _child_exists_under_parent(self, parent_subject, child_subject):
        xml = self._read()
        bounds = self._find_task_bounds_by_subject(xml, parent_subject)
        if bounds is None:
            return False

        open_start, close_start, close_end = bounds
        parent_block = xml[open_start:close_end]
        pattern = re.compile(rf'subject="{re.escape(child_subject)}"')
        return bool(pattern.search(parent_block))

    # --------------------------------------------------
    # Public API
    # --------------------------------------------------

    def reset_tasks(self):

        xml = self._read()

        guid_close = xml.find("</guid>")
        if guid_close == -1:
            raise Exception("Invalid file: missing </guid>")

        tasks_close = xml.rfind("</tasks>")
        if tasks_close == -1:
            raise Exception("Invalid file: missing </tasks>")

        header_part = xml[:guid_close + len("</guid>")]
        footer_part = xml[tasks_close:]

        new_xml = header_part + "\n\n" + footer_part

        self._write(new_xml)

    def add_category(self, name):

        xml = self._read()

        if f'subject="{name}"' in xml:
            return

        block = (
            f'<task creationDateTime="{self._now()}" '
            f'expandedContexts="(\'taskviewer\',)" '
            f'id="{self._new_id()}" '
            f'modificationDateTime="{self._now()}" '
            f'status="1" '
            f'subject="{name}">\n'
            f'</task>\n'
        )

        self._insert_root_task(block)

    def add_app(self, category, app_name):

        if self._child_exists_under_parent(category, app_name):
            return

        xml = self._read()
        if f'subject="{category}"' not in xml:
            self.add_category(category)

        block = (
            f'<task creationDateTime="{self._now()}" '
            f'expandedContexts="(\'taskviewer\',)" '
            f'id="{self._new_id()}" '
            f'modificationDateTime="{self._now()}" '
            f'status="1" '
            f'subject="{app_name}">\n'
            f'</task>\n'
        )

        self._insert_child(category, block)

    def add_testcase(self, app_name, testcase):

        if self._child_exists_under_parent(app_name, testcase):
            return

        block = (
            f'<task creationDateTime="{self._now()}" '
            f'id="{self._new_id()}" '
            f'modificationDateTime="{self._now()}" '
            f'status="1" '
            f'subject="{testcase}" />\n'
        )

        self._insert_child(app_name, block)

# --------------------------------------------------
# MAIN
# --------------------------------------------------

if __name__ == "__main__":

    source = Path("test-benign.tsk")
    copy_file = Path("test-benign-copy.tsk")

    if copy_file.exists():
        copy_file.unlink()

    shutil.copy2(source, copy_file)

    manager = TaskCoachManager(copy_file)
    manager.reset_tasks()

    import json

    with open("category_testcases.json", "r", encoding="utf-8") as f:
        category_testcases = json.load(f)

    csv_path = Path("apps_by_category.csv")

    if not csv_path.exists():
        raise FileNotFoundError("apps_by_category.csv not found")

    import pandas as pd
    df = pd.read_csv(csv_path)

    category_apps = {}

    for _, row in df.iterrows():
        category = str(row["Category"]).strip()
        app = str(row["App"]).strip()

        if category not in category_apps:
            category_apps[category] = []

        category_apps[category].append(app)


    for category, apps in category_apps.items():

        manager.add_category(category)

        for app in apps:
            manager.add_app(category, app)

            for tc in category_testcases.get(category, []):
                manager.add_testcase(app, tc)

    print("Done.")
