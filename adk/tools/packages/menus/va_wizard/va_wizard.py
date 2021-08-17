#!/usr/bin/env python
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.

# Python 2 and 3
import argparse
import os

from menus.wizard import (
    logger,
    Wizard,
    Step,
    gui
)

from menus.va_wizard.common import (
    installed_addons
)

from menus.va_wizard.ama import Ama
from menus.va_wizard.gaa import Gaa

PROVIDERS = [Ama, Gaa]


class Select(Step):
    def show(self):
        super(Select, self).show(title="Select VA providers to setup:")

        addons = installed_addons(self.cli_args.workspace)

        self.providers = dict()

        for p in PROVIDERS:
            if p['requires_addon']:
                if any(a == p['name'] for a in addons):
                    self.__show_provider(p)
            else:
                self.__show_provider(p)

        self.selected_providers = []

        for provider_name, provider in self.providers.items():
            data = provider['data']
            provider['var'].trace('w', self.__set_selected_providers)
            chk = gui.Checkbutton(self, offvalue='', onvalue=provider_name, variable=provider['var'], **data['display_opts'])
            chk.pack(anchor=gui.W, expand=True)

    def __show_provider(self, provider):
        self.providers[provider['name']] = {
            'data': provider,
            'var': gui.StringVar(self, name=provider['name'])
        }

    def __set_selected_providers(self, var_name, *args):
        provider_name = self.getvar(name=var_name)
        if provider_name:
            self.selected_providers.append(self.providers[var_name]['data'])
        else:
            self.selected_providers.remove(self.providers[var_name]['data'])

    def next(self):
        self.__add_steps_for_selected_providers()

        self.__remove_steps_for_deselected_providers()

    def __add_steps_for_selected_providers(self):
        for provider in self.selected_providers:
            for step in reversed(provider['steps']):
                if step not in self.wizard.steps:
                    self.wizard.steps.insert(1, step)

    def __remove_steps_for_deselected_providers(self):
        for provider in self.providers.values():
            provider_data = provider['data']
            if provider_data not in self.selected_providers:
                for step in provider_data['steps']:
                    if step in self.wizard.steps:
                        self.wizard.steps.remove(step)


class Finished(Step):
    def show(self):
        super(Finished, self).show(title="All Voice Assistants setup")


parser = argparse.ArgumentParser()
parser.add_argument("-k", "--kit", default="")
parser.add_argument("-w", "--workspace")


def run(args):
    wizard = Wizard(args, "Voice Assistant setup wizard", [Select, Finished])
    logger.log_to_file(os.path.join(os.path.dirname(args.workspace), 'va_setup_wizard.log'))
    wizard.run()


def main():
    args, _ = parser.parse_known_args()
    run(args)


if __name__ == "__main__":
    main()
