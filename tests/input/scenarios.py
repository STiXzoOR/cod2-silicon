"""Assertions on UI state after SDL events. Menu geometry comes from the game."""
import json
from contextlib import ExitStack
import os
from pathlib import Path
import re
import shlex
import subprocess
import socket
import time

from run import Driver, center, environment, find_item, find_menu, stop
from servers import FixtureServers


def candidates(state, predicate, visible=True):
    return [(menu['name'], item) for menu in state['menus'] if menu and (menu['visible'] or not visible)
            for item in menu['items'] if item and (not visible or item['visible'] and item['enabled']) and predicate(item)]


def opened_menus(script):
    tokens = shlex.split(script)
    return [tokens[n + 1] for n in range(len(tokens) - 1) if tokens[n].lower() == 'open']


def ui_scripts(script):
    tokens = shlex.split(script)
    return [tokens[n + 1] for n in range(len(tokens) - 1) if tokens[n].lower() == 'uiscript']


def list_point(item, row):
    box = item['list']
    assert 0 <= row < box['count'], f'Row {row} outside count {box["count"]}'
    x, y, width, height = item['rect']
    offset = row - box['start']
    inset = box.get('rowInset', 2)
    assert offset >= 0 and (offset + .5) * box['rowHeight'] + inset < height - box.get('paddingHeight', 0), f'Row {row} is not interactive'
    return x + min(30, width / 4), y + inset + (offset + .5) * box['rowHeight']


def list_capacity(item):
    box = item['list']
    return max(1, int((item['rect'][3] - box.get('paddingHeight', 0) - box.get('rowInset', 2)) / box['rowHeight']))


class Scenarios:
    def __init__(self, driver, send, results, fixtures):
        self.driver, self.send, self.results, self.fixtures = driver, send, results, fixtures

    def record(self, name, details):
        self.driver.snapshot(keep=True)
        self.results['checks'].append(dict(name=name, details=details, snapshot=str(self.driver.last_snapshot)))
        print('PASS ' + name, flush=True)

    def dismiss(self):
        for _ in range(5):
            state = self.driver.snapshot()
            safe = candidates(state, lambda it: 'close' in shlex.split(it['action']) and not any(token.lower() in ('uiscript', 'exec') for token in shlex.split(it['action'])))
            safe = [(menu, item) for menu, item in safe if 'popmenu' in menu]
            if not safe: return
            menu, item = safe[-1]; self.driver.click(menu, item['index'])

    def activate_script(self, script):
        state = self.driver.snapshot()
        assert state['uiContext']['keyCatchers'] >= 0, 'Client input ownership could not be observed'
        pattern = r'\b' + re.escape(script) + r'\b'
        direct = candidates(state, lambda it: any(re.search(pattern, name, re.I) for name in ui_scripts(it['action'])))
        if direct: return self.driver.click(direct[0][0], direct[0][1]['index'])
        target = [menu['name'] for menu in state['menus'] if menu and any(it and any(re.search(pattern, name, re.I) for name in ui_scripts(it['action'])) for it in menu['items'])]
        opener = candidates(state, lambda it: any(menu in target for menu in opened_menus(it['action'])))
        assert opener, f'No UI path to {script}'
        self.driver.click(opener[0][0], opener[0][1]['index'])
        return self.click_action(pattern)

    def screenshot(self, name):
        label = f'input-{len(self.results["checks"]):03d}-{name}'
        assert re.fullmatch(r'[\w.-]+', label)
        path = self.driver.output / 'home/main/screenshots' / (label + '.jpg')
        self.send('screenshotJPEG ' + label)
        self.driver.wait(lambda: path.exists() and path.stat().st_size > 0)
        self.results.setdefault('screenshots', []).append(str(path))

    def open(self, name):
        assert re.fullmatch(r'[\w.-]+', name), f'Unsafe runtime menu name {name!r}'
        if name.startswith('options_') and name != 'options_menu':
            state = self.open('options_menu')
            # Use the shipped panel actions so the parent menu and UI catcher
            # survive. Console-closing a focused child can release UI input.
            def panel_path(root):
                paths, visited = [(root, [])], set()
                while paths:
                    source, steps = paths.pop(0)
                    if source == name: return steps
                    if source in visited: continue
                    visited.add(source)
                    for item in find_menu(state, source)['items']:
                        if not item or (not steps and not (item['visible'] and item['enabled'])): continue
                        for target in opened_menus(item['action']):
                            if target.startswith('options_'):
                                paths.append((target, steps + [(source, item['index'], target)]))
            path = panel_path('options_menu')
            if not path:
                # Stock Multiplayer options have their own main-menu entry;
                # the identically placed Controls parent entry is hidden.
                state = self.open('main_text')
                path = panel_path('main_text')
            assert path, f'No runtime Options path to {name}'
            for source, index, target in path:
                state = self.driver.click(source, index)
                assert find_menu(state, target)['visible'], f'Options panel action failed: {target}'
            assert find_menu(state, name)['focused'], f'Options panel did not gain focus: {name}'
            return state
        state = self.driver.snapshot()
        assert state['uiContext']['keyCatchers'] >= 0, 'Client input ownership could not be observed'
        if state['uiContext']['editingField'] or state['uiContext']['waitingForKey']:
            self.driver.key('Escape')
            state = self.driver.snapshot()
            assert not state['uiContext']['editingField'] and not state['uiContext']['waitingForKey'], 'Escape did not finish pending UI input'
        if not state['uiContext']['keyCatchers'] & 8:
            assert state['client']['state'] == 0, 'Menu bootstrap requires a disconnected client'
            self.driver.key('Escape')
            state = self.driver.snapshot()
            assert state['uiContext']['keyCatchers'] >= 0 and state['uiContext']['keyCatchers'] & 8, 'Escape did not give input to the UI'
        commands = []
        for menu in reversed(state['menus']):
            if menu and menu['visible'] and menu['name'] not in ('main', 'bg'):
                assert re.fullmatch(r'[\w.-]+', menu['name']), 'Unsafe runtime menu name'
                commands.append('closemenu ' + menu['name'])
        commands.append('openmenu ' + name)
        self.send('; '.join(commands))
        self.driver.settle(180)
        state = self.driver.snapshot()
        assert find_menu(state, name)['visible'], f'Menu did not open: {name}'
        assert state['uiContext']['keyCatchers'] >= 0 and state['uiContext']['keyCatchers'] & 8, f'Opened menu does not own UI input: {name}'
        # Createserver opens its child panels; they own the resulting focus.
        assert any(m['focused'] and (m['name'] != 'main_text' or name == 'main_text') for m in state['menus'] if m and m['visible']), f'Scenario lost focus to main menu: {name}'
        return state

    def click_action(self, pattern, state=None):
        state = state or self.driver.snapshot()
        matches = candidates(state, lambda item: any(re.search(pattern, name, re.I) for name in ui_scripts(item['action'])))
        assert matches, f'No visible action matching {pattern!r}'
        menu, item = matches[0]
        return self.driver.click(menu, item['index'])

    def assert_labels(self, state, menu):
        labels = [item for item in find_menu(state, menu)['items'] if item and item['visible'] and item['text'].startswith('@')]
        assert labels, f'No visible localized labels in {menu}'
        for item in labels:
            assert item['resolvedText'] and item['resolvedText'] != item['text'][1:], f'Unresolved label: {menu}/{item["name"]}'
            if item['type'] in (0, 1): center(item['textRect'])
            # Label existence and localization are checked here; rendered glyph
            # evidence remains the screenshots/production opt-in text trace.
        self.record('labels/' + menu, {'localized': len(labels)})
        self.screenshot(menu)

    def controls(self, menus):
        tested = 0
        for name in menus:
            state = self.open(name)
            binds = candidates(state, lambda item: item['type'] == 14 and item['dvar'])
            if not binds: continue
            menu, item = binds[0]
            command = item['dvar']
            old = dict(state['bindings'])
            self.driver.click(menu, item['index'])
            assert self.driver.snapshot()['uiContext']['waitingForKey'] == 1, 'Bind control did not begin waiting for a key'
            self.driver.key('F8')
            state = self.driver.snapshot()
            assert state['uiContext']['waitingForKey'] == 0, 'F8 did not reach the pending bind handler'
            assert state['bindings'].get(str(0xae)) == command, f'Bind did not take F8: {command}'
            self.driver.click(menu, item['index']); self.driver.key('Backspace')
            state = self.driver.snapshot()
            assert state['uiContext']['waitingForKey'] == 0, 'Backspace did not finish bind clearing'
            assert command not in state['bindings'].values(), f'Bind clear failed: {command}'
            self.record('controls/' + name, {'command': command, 'boundKey': 'F8', 'cleared': True})
            # Restore only test-home bindings through console after assertions.
            for key, binding in old.items():
                if binding == command or key == str(0xae): self.send(f'bind 0x{int(key):02x} "{binding}"')
            tested += 1
        assert tested, 'No Controls bind items were exercised'

    def options(self, menus):
        totals = {11: 0, 12: 0, 10: 0}
        tabs = 0
        required_targets, tested_targets = set(), set()
        for name in menus:
            state = self.open(name)
            self.assert_labels(state, name)
            tab_items = [(menu, item) for menu, item in candidates(state, lambda item: any(re.search('option|control', name, re.I) for name in opened_menus(item['action'])))
                         if menu == name]
            for menu, tab in tab_items:
                target = [target for target in opened_menus(tab['action']) if re.search('option|control', target, re.I)]
                if menu in target: continue  # The current tab has no transition.
                required_targets.update(target)
                before = self.driver.snapshot()
                assert all(not find_menu(before, target_name)['visible'] for target_name in target), 'Tab target was already visible before activation'
                after = self.driver.click(menu, tab['index'])
                assert target and all(find_menu(after, target_name)['visible'] for target_name in target), f'Options tab failed {menu}/{tab["name"]}'
                assert any(find_menu(after, target_name)['focused'] for target_name in target), f'Options tab did not gain focus: {target}'
                tested_targets.update(target)
                tabs += 1
                self.open(name)
                state = self.driver.snapshot()
            items = [(menu, item) for menu, item in candidates(state, lambda item: item['type'] in totals and item['dvar'] and not item.get('redacted'))
                     if menu == name]
            for menu, original in items:
                state = self.driver.snapshot()
                item = find_item(state, menu, original['index'])
                if not item['visible'] or not item['enabled']: continue
                old = item['value']
                if item['type'] == 12 and len(item.get('choices', [])) < 2:
                    self.results.setdefault('skipped', []).append({'menu': menu, 'index': item['index'], 'reason': 'Only one supported choice'})
                    continue
                if item['type'] == 10:
                    slider = item.get('slider')
                    if not slider or slider['max'] <= slider['min']: continue
                    x, y, w, h = slider['rect']
                    fraction = .75 if abs(float(old or 0) - slider['min']) < (slider['max'] - slider['min']) / 2 else .25
                    self.driver.click_point((x + w * fraction, y + h / 2), state)
                else:
                    self.driver.click(menu, item['index'])
                after = find_item(self.driver.snapshot(), menu, item['index'])
                if any(script.lower() == 'verifylanguage' for script in ui_scripts(item['action'])):
                    supported = [str(choice['value']) for choice in item['choices'] if choice.get('available')]
                    assert supported, 'Language availability observer returned no installed language'
                    if len(supported) == 1:
                        assert after['value'] == supported[0], 'Unavailable language did not fall back to the installed language'
                        self.record(f'options/{menu}/language-retained', {'value': after['value'], 'installedChoices': 1})
                        self.results.setdefault('skipped', []).append({'menu': menu, 'index': item['index'], 'reason': 'Only one installed language; supported value retained after click'})
                        continue
                assert after['value'] != old, f'Option did not change: {menu}/{item["name"]}/{item["dvar"]}={old}'
                if item['type'] == 11:
                    assert float(after['value']) == (0 if float(old or 0) else 1), f'Yes/no toggle incorrect: {after}'
                if item['type'] == 12 and item.get('choices'):
                    choices = [str(c['value']) for c in item['choices']]
                    assert after['value'] in choices or any(str(c['value']) == after['value'] for c in item['choices']), f'Multi choice outside declared domain: {after}'
                if item['type'] == 10:
                    assert slider['min'] <= float(after['value']) <= slider['max'], f'Slider escaped range: {after}'
                self.record(f'options/{menu}/{item["name"]}', {'dvar': item['dvar'], 'before': old, 'after': after['value'], 'type': item['type']})
                self.send(f'set {item["dvar"]} "{old}"'); self.driver.settle(35)
                totals[item['type']] += 1
        assert all(totals.values()), f'Options coverage incomplete: {totals}'
        assert tabs, 'No Options tabs were clicked'
        assert required_targets <= tested_targets, f'Options targets not exercised: {required_targets - tested_targets}'
        self.record('options/coverage', {'yesNo': totals[11], 'multi': totals[12], 'sliders': totals[10], 'tabs': tabs, 'targets': sorted(tested_targets)})

    def select_row(self, menu, item_name, row, double=False, require_change=True):
        state = self.driver.snapshot(); item = find_item(state, menu, item_name)
        self.driver.move(center(item['rect']), state)
        for _ in range(80):
            state = self.driver.snapshot(); item = find_item(state, menu, item_name); box = item['list']
            capacity = list_capacity(item)
            if box['start'] <= row < box['start'] + capacity: break
            self.driver.wheel(-.25 if row >= box['start'] + capacity else .25, repeats=4)
        else: raise AssertionError(f'Cannot scroll to row {row}')
        if box['selected'] == row and require_change:
            alternate = box['start'] if box['start'] != row else min(box['end'], row + 1)
            assert alternate != row, 'List offers no different visible row for a meaningful click assertion'
            self.driver.click_point(list_point(item, alternate), state)
            state = self.driver.snapshot(); item = find_item(state, menu, item_name)
            assert item['list']['selected'] == alternate, 'Preparing alternate row did not change selection'
        point = list_point(item, row)
        self.driver.move(point, state)
        hovered = self.driver.snapshot(keep=True)
        hover = find_item(hovered, menu, item_name)['list']['cursor']
        assert hover == row, f'Pointer over row {row} resolves hover row {hover}'
        if require_change: assert find_item(hovered, menu, item_name)['list']['selected'] != row, 'Target was selected before the tested click'
        self.driver.click_point(point, hovered, double=double)
        after = self.driver.snapshot()
        if not double:
            box = find_item(after, menu, item_name)['list']
            assert box['selected'] == row and box['cursor'] == row, f'Row {row} selection incorrect: {box}'
        return after

    def start_server(self, name, launch=False):
        state = self.open(name)
        matches = candidates(state, lambda item: item['type'] == 6 and item.get('list', {}).get('feeder') == 4)
        assert matches, 'Start New Server map list missing'
        menu, item = matches[0]
        count = item['list']['count']; assert count >= 2, 'Not enough maps loaded'
        row = min(count - 1, 2)
        state = self.select_row(menu, item['index'], row)
        expected = [entry for entry in state['maps'] if entry['active']][row]
        assert int(state['dvars']['ui_currentNetMap']) == expected['index'], 'Map click did not update ui_currentNetMap'
        self.record('startserver/map-selection', {'menu': menu, 'row': row, 'count': count, 'map': expected['loadName']})
        choices = candidates(state, lambda item: item['type'] in (11, 12) and item['dvar'])
        assert choices, 'Start New Server multi options missing'
        for option_menu, option in choices:
            before = option['value']; after = self.driver.click(option_menu, option['index'])
            changed = find_item(after, option_menu, option['index'])
            assert changed['value'] != before, f'Start New Server choice did not change: {option["dvar"]}'
            self.record('startserver/choice/' + option['name'], {'dvar': option['dvar'], 'before': before, 'after': changed['value']})
            self.send(f'set {option["dvar"]} "{before}"'); self.driver.settle(35)
        if launch:
            assert self.driver.snapshot()['client']['state'] == 0, 'Listen test began with an active connection'
            # A gametype choice can rebuild the map feeder. Select again after
            # restoring choices so the launch checks the current UI selection.
            state = self.select_row(menu, item['index'], row)
            expected = [entry for entry in state['maps'] if entry['active']][row]
            assert int(state['dvars']['ui_currentNetMap']) == expected['index'], 'Listen launch lost the selected map'
            self.send('set dedicated 0; set ui_dedicated 0; set sv_pure 0; set sv_maxclients 1')
            self.click_action(r'\bStartServer\b')
            self.driver.wait(lambda: self.driver.snapshot().get('client', {}).get('state') == 8, 50)
            active = self.driver.snapshot()
            assert active['dvars']['sv_running'] == '1' and active['dvars']['mapname'] == expected['loadName'], 'Listen server did not run the selected map'
            self.record('startserver/listen-active', {'state': 8, 'map': expected['loadName'], 'sv_running': '1'})
            self.screenshot('listen-active')

    def browser(self, name, join=False):
        self.send('set ui_netSource 0; set ui_browserShowPure 0')
        state = self.open(name)
        def populated():
            servers = self.driver.snapshot().get('servers', {})
            return servers.get('displayCount', 0) >= 24 and not servers.get('refreshing', True)
        def refresh():
            self.click_action(r'\bRefreshServers\b')
            self.fixtures.announce()
            self.driver.wait(lambda: self.driver.snapshot().get('servers', {}).get('count', 0) >= 24, 8)
            self.driver.snapshot(keep=True)
            # New LAN responses enter as clean slots in the stock parser. The
            # actual Quick Refresh action marks the now-present slots for ping.
            self.click_action(r'\bRefreshFilter\b')
            self.driver.snapshot(keep=True)
            self.driver.wait(populated, 12)
            self.driver.settle(100)
        refresh()
        state = self.driver.snapshot()
        self.assert_labels(state, name)
        state = self.driver.snapshot()
        lists = candidates(state, lambda item: item['type'] == 6 and item.get('list', {}).get('feeder') == 2)
        assert lists, 'Join Server list missing'
        menu, item = lists[0]
        before = item['list']['start']
        # Selectable stock listboxes move their selection by one wheel step;
        # the viewport advances only when that selection crosses its edge.
        edge = item['list']['start'] + list_capacity(item) - 1
        state = self.select_row(menu, item['index'], edge)
        self.driver.move(center(item['rect']), state)
        self.driver.wheel(-.25, flipped=True, repeats=4)
        box = find_item(self.driver.snapshot(), menu, item['index'])['list']
        assert box['selected'] == edge + 1, 'Four quarter-wheel events did not produce one selection step'
        self.driver.wheel(-.25, flipped=True, repeats=4)
        box = find_item(self.driver.snapshot(), menu, item['index'])['list']
        scrolled = box['start']
        assert scrolled > before, f'Fractional trackpad scroll did not advance list: {before}->{scrolled}'
        self.driver.wheel(.25, flipped=True, repeats=4)
        box = find_item(self.driver.snapshot(), menu, item['index'])['list']
        assert box['selected'] == edge + 1, 'Trackpad reversal did not reverse selection'
        self.driver.wheel(.25, flipped=True, repeats=4)
        for _ in range(32):
            current = find_item(self.driver.snapshot(), menu, item['index'])['list']
            if current['selected'] == 0 and current['start'] == 0: break
            self.driver.wheel(.25, flipped=True, repeats=4)
        else: raise AssertionError('Trackpad reversal did not return selection and viewport to the beginning')
        returned = find_item(self.driver.snapshot(), menu, item['index'])['list']['start']
        assert returned < scrolled, 'Trackpad scroll reversal did not move toward list start'
        capacity = list_capacity(item)
        row = min(item['list']['count'] - 1, capacity + 2)
        selected = self.select_row(menu, item['index'], row)
        assert selected['servers']['selected'] == row, 'Browser selected a different server than clicked row'
        self.record('joinserver/local-scroll-select', {'row': row, 'count': item['list']['count'], 'startBefore': before, 'startAfter': scrolled})
        self.screenshot('joinserver-row-' + str(row))
        for kind in ('filter', 'serverinfo'):
            state = self.driver.snapshot()
            opener = candidates(state, lambda it: any(kind in name.lower() for name in opened_menus(it['action'])))
            assert opener, f'{kind} dialog opener missing'
            dialog_state = self.driver.click(opener[0][0], opener[0][1]['index'])
            dialog = next(name for name in opened_menus(opener[0][1]['action']) if kind in name.lower())
            self.assert_labels(dialog_state, dialog)
            if kind == 'filter':
                toggles = [(m, it) for m, it in candidates(dialog_state, lambda it: it['type'] in (11, 12) and it['dvar']) if m == dialog]
                assert toggles, 'Filter fields missing'
                for dialog_menu, toggle in toggles:
                    if toggle['type'] == 12 and len(toggle.get('choices', [])) < 2: continue
                    changed = self.driver.click(dialog_menu, toggle['index'])
                    value = find_item(changed, dialog_menu, toggle['index'])['value']
                    assert value != toggle['value'], f'Filter did not change {toggle["dvar"]}'
                    self.send(f'set {toggle["dvar"]} "{toggle["value"]}"'); self.driver.settle(35)
                self.record('filter/fields', {'count': len(toggles)})
            elif self.fixtures.target:
                self.driver.wait(lambda: any(it.get('list', {}).get('count', 0) > 0 for m, it in candidates(self.driver.snapshot(), lambda it: it['type'] == 6 and it.get('list', {}).get('feeder') == 13) if m == dialog), 10)
                self.record('serverinfo/real-status', {'upstream': f'{self.fixtures.target[0]}:{self.fixtures.target[1]}'})
            self.dismiss()
            # Filter onClose may refresh Local, so reannounce fixtures before
            # proceeding to Favorites and restore the selected row.
            if kind == 'filter':
                refresh()
                self.select_row(menu, item['index'], row)
        # Add/remove through menu actions, with source changing through its UI.
        state = self.driver.snapshot()
        advertised = state['servers']['addresses'][state['servers']['selected']]
        old_favorites = set(state['favorites'])
        self.activate_script('addFavorite'); self.dismiss()
        assert advertised in self.driver.snapshot()['favorites'], 'Favorite added a different address than the selected row'
        source = candidates(self.driver.snapshot(), lambda it: it.get('ownerDraw') == 220)
        assert source, 'Browser source control missing'
        source_menu, source_item = source[0]
        for _ in range(3):
            state = self.driver.snapshot()
            if state['dvars']['ui_netSource'] == '2': break
            self.driver.click(source_menu, source_item['index'])
        state = self.driver.snapshot()
        assert state['dvars']['ui_netSource'] == '2', 'Favorites source did not activate'
        self.driver.wait(lambda: self.driver.snapshot()['servers']['displayCount'] >= 1, 8)
        state = self.driver.snapshot()
        favorite_row = state['servers']['addresses'].index(advertised)
        self.select_row(menu, item['index'], favorite_row, require_change=state['servers']['displayCount'] > 1)
        previous_count = self.driver.snapshot()['servers']['count']
        self.activate_script('deleteFavorite'); self.dismiss()
        state = self.driver.snapshot()
        assert state['servers']['count'] == previous_count - 1, 'Favorite was not removed'
        assert advertised not in state['favorites'], 'Favorite removed a different address than the selected row'
        assert set(state['favorites']) == old_favorites - {advertised}, 'Favorites changed unrelated addresses'
        self.record('joinserver/favorites-add-remove', {'address': advertised, 'countAfter': state['servers']['count']})
        if join:
            assert self.fixtures.target, 'A real localhost --server-address is required for joins'
            # Restore Local with clicks, refresh real parser rows, then exercise
            # both activation paths against the same real server via UDP proxies.
            for activation in ('button', 'double-click'):
                self.send('disconnect; set ui_netSource 0')
                self.driver.settle(350)
                assert self.driver.snapshot()['client']['state'] == 0, 'Join test began with an active connection'
                self.open(name); refresh()
                expected_address = self.driver.snapshot()['servers']['addresses'][row]
                self.select_row(menu, item['index'], row, double=activation == 'double-click')
                if activation == 'button':
                    assert self.driver.snapshot()['client']['state'] == 0, 'Selecting a row unexpectedly joined a server'
                    self.click_action(r'\bJoinServer\b')
                self.driver.wait(lambda: self.driver.snapshot().get('client', {}).get('state') == 8, 50)
                assert self.driver.snapshot()['client']['server'] == expected_address, 'Joined a different advertised row address'
                self.record('joinserver/' + activation, {'row': row, 'address': expected_address, 'clientState': 8, 'upstream': f'{self.fixtures.target[0]}:{self.fixtures.target[1]}'})
                self.screenshot('joined-' + activation)

    def relative_gain(self, menu):
        state = self.open(menu)
        assert state['mouse']['relative'], 'Fullscreen menu did not retain SDL relative input'
        assert state['dvars']['in_rawmouse'] == '1', 'Fullscreen test did not enable raw preference'
        self.driver.move((state['render'][0] / 2, state['render'][1] / 2), state)
        before = self.driver.snapshot()
        delta = [10, -6]
        self.driver.relative(*delta)
        after = self.driver.snapshot()
        actual = [after['cursor'][n] - before['cursor'][n] for n in range(2)]
        assert actual == delta, f'Fullscreen menu SDL delta changed: injected {delta}, UI {actual}'
        pixels = [after['cursorPixels'][n] - before['cursorPixels'][n] for n in range(2)]
        points = [pixels[n] * after['contentPoints'][n] / after['render'][n] for n in range(2)]
        self.record('fullscreen/relative-menu-gain', {'injectedSDLRelative': delta, 'uiDelta': actual,
                    'renderDelta': pixels, 'contentPointDelta': points,
                    'contentPointGain': [points[n] / delta[n] for n in range(2)],
                    'window': after['window'], 'render': after['render'], 'desktopPoints': after['desktopPoints'],
                    'contentPoints': after['contentPoints'], 'backingScale': after['backingScale'],
                    'rawPreference': after['dvars']['in_rawmouse'], 'm_rinput': after['dvars']['m_rinput'], 'com_maxfps': after['dvars']['com_maxfps']})

    def quit_confirm(self):
        self.send('disconnect')
        self.driver.settle(300)
        state = self.open('quit_popmenu')
        matches = candidates(state, lambda item: any(name.lower() == 'quit' for name in ui_scripts(item['action'])))
        assert matches, 'Quit dialog confirm action missing'
        menu, item = matches[0]
        self.driver.snapshot(keep=True)
        checkpoint = str(self.driver.last_snapshot)
        self.driver.move(center(item['hitRect']), state)
        sequence = self.driver.queue('button 1 1 1')
        code = self.driver.process.wait(timeout=8)
        assert code == 0, f'Quit confirm returned exit {code}'
        assert self.driver.acks().get(sequence, {}).get('status') == 1, 'Quit confirm press did not enter SDL queue'
        self.results['checks'].append({'name': 'quit/confirm-clean-exit', 'details': {'exitCode': code}, 'snapshot': checkpoint})
        print('PASS quit/confirm-clean-exit', flush=True)

    def dialogs(self, names):
        password = 0
        for name in names:
            state = self.open(name)
            self.assert_labels(state, name)
            edits = [(menu, item) for menu, item in candidates(state, lambda item: item['type'] == 4 and item.get('redacted') and 'password' in item['dvar'].lower())
                     if menu == name]
            for menu, item in edits:
                self.driver.click(menu, item['index'])
                for _ in range(item.get('valueLength', 0)): self.driver.key('Backspace', 10)
                self.driver.text('WS42-test')
                after = find_item(self.driver.snapshot(), menu, item['index'])
                assert after.get('valueLength') == 9 and after.get('redacted'), f'Password text input failed {menu}/{item["name"]}'
                self.record('password/text-input', {'menu': menu, 'length': 9})
                password += 1
            if 'quit' in name.lower():
                quits = candidates(state, lambda item: any(name.lower() == 'quit' for name in ui_scripts(item['action'])))
                assert quits, 'Quit dialog confirm action missing'
                for menu, item in quits: center(item['hitRect'])
                self.driver.key('Escape')
                assert not find_menu(self.driver.snapshot(), name)['visible'], 'Quit dialog cancellation failed'
                self.record('quit/cancel', {'menu': name})
        assert password, 'No password dialog was exercised'


def owned_server(args, output):
    port_check = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: port_check.bind(('0.0.0.0', args.server_port))
    finally: port_check.close()
    server_output = output / 'server'; server_output.mkdir()
    command = ['timeout', '-k', '5', '300', str(args.server_binary.resolve()),
               '+set', 'fs_basepath', str(args.base.expanduser().resolve()),
               '+set', 'fs_homepath', str(server_output / 'home'),
               '+set', 'net_port', str(args.server_port), '+set', 'dedicated', '1',
               '+set', 'sv_pure', '0', '+set', 'sv_maxclients', '8',
               '+set', 'sv_hostname', 'WS42 local input fixture',
               '+set', 'sv_lanOnly', '1', '+set', 'g_gametype', 'dm', '+map', 'mp_toujane']
    with (server_output / 'console.log').open('w') as log:
        process = subprocess.Popen(command, cwd=server_output, stdout=log, stderr=log, start_new_session=True)
    udp = None
    try:
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); udp.settimeout(.2)
        target = ('127.0.0.1', args.server_port)
        deadline = time.monotonic() + 50
        while time.monotonic() < deadline:
            if process.poll() is not None: raise RuntimeError('Owned dedicated server exited during startup')
            udp.sendto(b'\xff' * 4 + b'getinfo WS42', target)
            try: response, peer = udp.recvfrom(4096)
            except socket.timeout: continue
            if peer == target and response.startswith(b'\xff' * 4 + b'infoResponse\n') and b'\\mapname\\mp_toujane' in response:
                return process, f'127.0.0.1:{args.server_port}'
        raise RuntimeError('Owned dedicated server did not become ready')
    except Exception:
        stop(process); raise
    finally:
        if udp: udp.close()


def retain_processes(output, stage):
    found = subprocess.run(['pgrep', '-fl', 'cod2_macos'], capture_output=True, text=True, check=False)
    text = re.sub(r'(?i)(\+set\s+\S*(?:password|cdkey|codkey|hwid|guid)\S*\s+)("[^"]*"|\S+)', r'\1[redacted]', found.stdout)
    with (output / 'processes.log').open('a') as stream:
        stream.write(f'{time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())} {stage}: pgrep -fl cod2_macos returned {found.returncode}\n')
        stream.write(text or '(no matching processes)\n')


def live(args, output, probe):
    retain_processes(output, 'before waiting for display')
    deadline = time.monotonic() + 180
    announced = False
    while subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
        if not announced: print('Waiting for the existing game to exit; no process is changed.', flush=True); announced = True
        if time.monotonic() >= deadline: raise RuntimeError('Another game remains running after 180s; no process was changed')
        time.sleep(1)
    lifetime = '60' if args.fullscreen else ('25' if args.scenario == 'inventory' else '180')
    command = ['timeout', '-k', '5', lifetime, str(args.binary.resolve()),
               '+set', 'fs_basepath', str(args.base.expanduser().resolve()),
               '+set', 'fs_homepath', str(output / 'home'), '+set', 'net_port', str(args.port),
               '+set', 'r_fullscreen', '1' if args.fullscreen else '0', '+set', 'r_mode', '1920x1080' if args.fullscreen else '1280x720',
               '+set', 'developer', '1', '+set', 'com_introPlayed', '1', '+set', 'com_maxfps', '333' if args.fullscreen else '60',
               '+set', 'ui_netSource', '0', '+set', 'sv_pure', '0',
               '+set', 'ui_playerProfileAlreadyChosen', '1',
               '+set', 'in_rawmouse', '1' if args.fullscreen else '0', '+set', 'm_rinput', '1']
    env = environment(output, probe, args.sdl3)
    env.pop('COD2_UI_SWEEP', None)
    env.pop('COD2_NATIVE_TRACE', None)
    env.pop('COD2_TEST_INPUT_TRACE_TEXT', None)
    if args.native_trace: env['COD2_NATIVE_TRACE'] = '1'
    results = {'completed': False, 'checks': [], 'mode': args.scenario,
               'fullscreen': args.fullscreen, 'browseRows': 'localhost simulated identities; real getinfo and UDP joins when upstream supplied',
               'realJoinServer': args.server_address or None}
    client_started = None
    try:
        with ExitStack() as resources:
            browser_resources = resources.enter_context(ExitStack())
            target = args.server_address
            if args.server_binary:
                server_process, target = owned_server(args, output)
                browser_resources.callback(stop, server_process)
                results['realJoinServer'] = target
                results['serverFixture'] = 'owned dedicated server (Start New Server separately tests a listen server)'
            fixtures = FixtureServers(args.port, target=target)
            browser_resources.callback(fixtures.close)
            fixtures.start()
            stream = resources.enter_context((output / 'console.log').open('w'))
            retain_processes(output, 'before client launch')
            if subprocess.run(['pgrep', '-x', 'cod2_macos'], capture_output=True).returncode != 1:
                raise RuntimeError('Another game started while the private server was loading; no process was changed')
            process = subprocess.Popen(command, env=env, cwd=output, stdin=subprocess.PIPE,
                                       stdout=stream, stderr=stream, text=True, start_new_session=True)
            resources.callback(stop, process)
            client_started = time.monotonic()
            def send(text):
                if process.poll() is not None: raise RuntimeError('Game exited')
                process.stdin.write(text + '\n'); process.stdin.flush()
            driver = Driver(process, output)
            resources.callback(driver.close)
            driver.settle(1500)
            state = driver.snapshot(keep=True)
            inventory = [menu['name'] for menu in state['menus'] if menu]
            (output / 'inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')
            print('Runtime menus: ' + ', '.join(inventory), flush=True)
            suite = Scenarios(driver, send, results, fixtures)
            if args.scenario in ('browser', 'menus', 'join', 'all'):
                join = [name for name in inventory if name.lower() == 'joinserver']
                assert len(join) == 1, 'Join Server menu missing'
                if args.fullscreen: suite.relative_gain(join[0])
                suite.browser(join[0], join=args.scenario in ('join', 'all'))
                send('disconnect')
                driver.settle(300)
                assert driver.snapshot()['client']['state'] == 0, 'Browser fixture cleanup requires a disconnected client'
                browser_resources.close()
            if args.scenario in ('menus', 'all', 'options', 'controls', 'maps', 'dialogs'):
                send('disconnect')
                driver.settle(300)
                options = [name for name in inventory if 'option' in name.lower() and not any(word in name.lower() for word in ('ingame', 'reset', 'popmenu', 'defaults'))]
                controls = [menu['name'] for menu in state['menus'] if menu and any(item and item['type'] == 14 for item in menu['items'])]
                if args.scenario in ('menus', 'all', 'options'): suite.options(options)
                if args.scenario in ('menus', 'all', 'controls'): suite.controls(controls)
                start = [name for name in inventory if any(word in name.lower() for word in ('startserver', 'createserver'))]
                assert start, 'Start New Server menu missing'
                if args.scenario in ('menus', 'all', 'maps'): suite.start_server(start[0], launch=args.scenario == 'maps')
                dialogs = [name for name in inventory if any(word in name.lower() for word in ('filter', 'serverinfo', 'password', 'quit'))]
                if args.scenario in ('menus', 'all', 'dialogs'): suite.dialogs(dialogs)
                if args.scenario == 'all': suite.start_server(start[0], launch=True)
            if args.scenario != 'inventory': suite.quit_confirm()
            results['completed'] = True
    except Exception as error:
        results['failure'] = str(error)
        raise
    finally:
        # ExitStack has already stopped every acquired process/socket, even
        # when setup or cleanup fails. A disk error cannot bypass those stops.
        if client_started is not None: results['clientElapsedSeconds'] = round(time.monotonic() - client_started, 3)
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    if args.scenario == 'inventory': print(f'Captured {len(inventory)} runtime menus; {output}', flush=True)
    else: print(f'PASS {len(results["checks"])} live input checks; {output}', flush=True)
