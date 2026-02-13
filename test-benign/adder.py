import uuid
from datetime import datetime
from pathlib import Path
import shutil


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

        marker = f'subject="{parent_subject}"'
        start = xml.find(marker)

        if start == -1:
            return

        close_tag = xml.find("</task>", start)

        xml = xml[:close_tag] + "\n" + child_block + xml[close_tag:]

        self._write(xml)

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

        xml = self._read()

        if f'subject="{app_name}"' in xml:
            return

        if f'subject="{category}"' not in xml:
            self.add_category(category)
            xml = self._read()

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

        xml = self._read()

        if f'subject="{testcase}"' in xml:
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

    manager.add_category("Office")
    manager.add_app("Office", "Word")
    manager.add_testcase("Word", "Save file")

    manager.add_category("Browser")
    manager.add_app("Browser", "Chrome")
    manager.add_testcase("Chrome", "Downloads")
    manager.add_testcase("Chrome", "Browsing")

    print("Done.")
