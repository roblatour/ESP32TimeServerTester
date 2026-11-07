import contextlib
import io
import unittest
from unittest import mock

from tools import ote_upload


class PasswordArgumentTests(unittest.TestCase):
    arguments = [
        "-i", "ESP32TimeServerTester.local", "-p", "3232",
        "-f", r"build\ESP32TimeServerTester.bin",
    ]

    def run_main(self, extra, environment, interactive=False):
        output = io.StringIO()
        errors = io.StringIO()
        with (
            mock.patch.dict(ote_upload.os.environ, environment, clear=True),
            mock.patch.object(ote_upload.sys.stdin, "isatty", return_value=interactive),
            mock.patch.object(ote_upload.getpass, "getpass", return_value="prompt-password") as prompt,
            mock.patch.object(ote_upload, "upload_firmware", return_value="Image accepted") as upload,
            contextlib.redirect_stdout(output),
            contextlib.redirect_stderr(errors),
        ):
            result = ote_upload.main(self.arguments + extra)
        return result, upload, prompt, output.getvalue() + errors.getvalue()

    def test_argument_overrides_environment_without_prompting(self):
        for option in ("-a", "--password"):
            with self.subTest(option=option):
                result, upload, prompt, output = self.run_main(
                    [option, "argument-password"], {"OTE_PASSWORD": "environment-password"}
                )
                self.assertEqual(result, 0)
                self.assertEqual(upload.call_args.args[2], "argument-password")
                prompt.assert_not_called()
                self.assertNotIn("argument-password", output)

    def test_argument_works_without_environment_or_terminal(self):
        result, upload, prompt, _ = self.run_main(["-a", "argument-password"], {})
        self.assertEqual(result, 0)
        self.assertEqual(upload.call_args.args[2], "argument-password")
        prompt.assert_not_called()

    def test_environment_fallback_is_preserved(self):
        for extra, environment in (
            ([], {"OTE_PASSWORD": "environment-password"}),
            (["--password-env", "CUSTOM_OTE_PASSWORD"], {"CUSTOM_OTE_PASSWORD": "environment-password"}),
        ):
            with self.subTest(extra=extra):
                result, upload, prompt, _ = self.run_main(extra, environment)
                self.assertEqual(result, 0)
                self.assertEqual(upload.call_args.args[2], "environment-password")
                prompt.assert_not_called()

    def test_prompt_fallback_is_preserved(self):
        result, upload, prompt, _ = self.run_main([], {}, interactive=True)
        self.assertEqual(result, 0)
        self.assertEqual(upload.call_args.args[2], "prompt-password")
        prompt.assert_called_once()

    def test_missing_noninteractive_password_explains_both_options(self):
        result, upload, prompt, output = self.run_main([], {})
        self.assertEqual(result, 1)
        upload.assert_not_called()
        prompt.assert_not_called()
        self.assertIn("-a PASSWORD", output)
        self.assertIn("OTE_PASSWORD", output)

    def test_empty_argument_does_not_fall_back_to_environment(self):
        with (
            mock.patch.dict(ote_upload.os.environ, {"OTE_PASSWORD": "environment-password"}, clear=True),
            mock.patch.object(ote_upload.getpass, "getpass") as prompt,
            contextlib.redirect_stdout(io.StringIO()),
            contextlib.redirect_stderr(io.StringIO()) as errors,
        ):
            result = ote_upload.main(self.arguments + ["-a", ""])
        self.assertEqual(result, 1)
        self.assertIn("password must be nonempty", errors.getvalue())
        prompt.assert_not_called()


if __name__ == "__main__":
    unittest.main()
