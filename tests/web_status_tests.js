'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const page = fs.readFileSync('rendered_page.html', 'utf8');
const script = page.match(/<script>([\s\S]*?)<\/script>/)?.[1];
assert.ok(script, 'rendered page contains its script');
new vm.Script(script);

const buttons = [...page.matchAll(/<button class="test-run" onclick="run\('([^']+)'\)" disabled>([^<]+)<\/button>/g)];
assert.deepEqual(buttons.map(match => match[1]), [
    'all', 'ping', 'ntp', 'ipv4', 'ipv6', 'ptp-l2', 'ptp-auto', 'ptp-assist',
    'ptp-l3', 'ptp-l3-auto', 'ptp-l3-assist', 'ptp'
]);
assert.equal(buttons[5][2], 'Run all PTP L2 tests');
assert.equal(buttons[8][2], 'Run all PTP L3 UDP tests');
assert.equal(buttons[11][2], 'Run all PTP tests');
const modeSection = page.match(/<select id="firmware-mode"[\s\S]*?<\/select>/)?.[0];
assert.ok(modeSection);
assert.deepEqual([...modeSection.matchAll(/<option value="([^"]+)"/g)].map(match => match[1]), [
    'Unknown', 'NtpOnly', 'PtpOnly', 'PtpL3Only', 'Combined',
    'NtpPtpL3', 'PtpL2PtpL3', 'NtpPtpL2PtpL3'
]);
assert.match(modeSection, /value="NtpPtpL2PtpL3" selected/);
for (const field of ['ptp_udp_ipv4', 'ptp_udp_ipv6', 'ptp_udp_domain', 'ptp_udp_withdrawal'])
    assert.ok(page.includes(`name="${field}"`), `generated form includes ${field}`);
assert.ok(!page.includes('name="ptp_udp_key'), 'UDP transport has no symmetric-key field');

class Element {
    constructor(tag = '') {
        this.tagName = tag.toUpperCase();
        this.children = [];
        this.style = {};
        this.disabled = false;
        this.hidden = false;
        this.textContent = '';
        this.className = '';
        this.checked = false;
        this.value = '';
    }
    insertCell() {
        const cell = new Element('td');
        this.children.push(cell);
        return cell;
    }
    append(...children) { this.children.push(...children); }
    replaceChildren(...children) { this.children = children; }
}

const elements = new Map();
const getElement = id => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
};
const runButtons = buttons.map(() => new Element('button'));
const context = {
    document: {
        getElementById: getElement,
        querySelectorAll: selector => selector === '.test-run' ? runButtons : [],
        createElement: tag => new Element(tag)
    },
    savedOption: () => ({}),
    setWebLedsFlashing: () => {},
    webLedColors: {},
    run: () => {}
};
const start = script.indexOf('function renderStatus(j){');
const end = script.indexOf('function renderConnectionLost(){', start);
assert.ok(start >= 0 && end > start);
vm.createContext(context);
new vm.Script(`${script.slice(start, end)}\nthis.renderStatus=renderStatus;`).runInContext(context);

const test = (id, name, flags = {}) => ({
    id, number: id + 1, name, family: 0, enabled: true, compatible: true,
    ptp: false, ptpL2: false, ptpL3: false, ntp: false, assisted: false,
    status: 'Not yet run', time: '', ...flags
});
const status = {
    running: false, target_valid: true, ready: true, link: true, ipv4: true, ipv6: 'lla',
    network: {mac: '02:11:22:33:44:55', ipv4: '192.0.2.10', lla: 'fe80::10', ula: '', gua: ''},
    physical_leds_flashing: false, physical_leds_flash_on: false,
    ptp_policy: 'policy', ptp_preflight: 'preflight', active_test: -1,
    phase_waiting: false, phase_prompt: '', progress: 'Idle',
    tests: [
        test(0, 'Ping', {family: 4}),
        test(1, 'NTP', {family: 4, ntp: true}),
        test(2, 'PTP L2 auto', {ptp: true, ptpL2: true}),
        test(3, 'PTP L2 assisted', {ptp: true, ptpL2: true, assisted: true}),
        test(4, 'PTP L3 UDP IPv4', {family: 4, ptp: true, ptpL3: true}),
        test(5, 'PTP L3 UDP IPv6 assisted', {family: 6, ptp: true, ptpL3: true, assisted: true, enabled: false}),
        test(6, 'PTP L3 UDP dual-stack', {ptp: true, ptpL3: true, enabled: false})
    ]
};
getElement('save-settings').disabled = true;
context.renderStatus(status);
assert.equal(runButtons[8].disabled, false, 'configured IPv4 enables the L3 group');
assert.equal(runButtons[9].disabled, false, 'configured IPv4 enables the L3 unattended group');
assert.equal(runButtons[10].disabled, true, 'disabled IPv6 cannot enable the L3 assisted group');
assert.equal(runButtons[11].disabled, false, 'all PTP includes the available L2 and L3 tests');
let rows = getElement('tests').children;
assert.equal(rows[4].children[1].children[0].disabled, false);
assert.equal(rows[5].children[1].children[0].disabled, true);
assert.equal(rows[6].children[1].children[0].disabled, true);

status.tests[4].enabled = false;
context.renderStatus(status);
assert.equal(runButtons[8].disabled, true, 'no configured family disables all L3 tests');
assert.equal(runButtons[11].disabled, false, 'all PTP remains available for L2');
status.tests[2].enabled = status.tests[3].enabled = false;
context.renderStatus(status);
assert.equal(runButtons[11].disabled, true, 'all PTP is disabled when neither transport is available');

status.tests[5].enabled = true;
context.renderStatus(status);
assert.equal(runButtons[8].disabled, false, 'IPv6 alone enables the L3 group');
assert.equal(runButtons[9].disabled, true, 'no enabled unattended UDP test disables its group');
assert.equal(runButtons[10].disabled, false, 'configured IPv6 enables the L3 assisted group');
assert.equal(runButtons[11].disabled, false, 'all PTP also works for an L3-only profile');
status.running = true;
status.tests[5].status = 'Running';
status.tests[5].time = 'Live UDP evidence';
context.renderStatus(status);
assert.ok(runButtons.every(button => button.disabled), 'a running test disables every run button');
assert.equal(getElement('stop-tests').disabled, false);
rows = getElement('tests').children;
assert.equal(rows[5].children[1].children[1].textContent, 'Running');
assert.equal(rows[5].children[2].textContent, 'Live UDP evidence');
status.running = false;
getElement('save-settings').disabled = false;
context.renderStatus(status);
assert.ok(runButtons.every(button => button.disabled), 'unsaved settings block test execution');

const profileStart = script.indexOf('function loadProfile(option){');
const profileEnd = script.indexOf('async function selectTarget(', profileStart);
assert.ok(profileStart >= 0 && profileEnd > profileStart);
const fields = {};
for (const name of ['firmwareMode', 'ipv4', 'ipv6', 'ipv4Port', 'ipv6Port', 'keyId', 'keyValue',
    'stress', 'ptpMac', 'ptpDomain', 'ptpKeyId', 'ptpKeyValue', 'ptpUdpIpv4', 'ptpUdpIpv6',
    'ptpUdpDomain', 'ptpUdpWithdrawal']) fields[name] = new Element();
const profileContext = {profileFields: fields, updateSaveButton: () => {}};
vm.createContext(profileContext);
new vm.Script(`${script.slice(profileStart, profileEnd)}\nthis.loadProfile=loadProfile;`).runInContext(profileContext);
profileContext.loadProfile({dataset: {
    firmwareMode: 'PtpL3Only', ptpUdpIpv4: '192.0.2.20', ptpUdpIpv6: 'fe80::20',
    ptpUdpDomain: '17', ptpUdpWithdrawal: '25000'
}});
assert.equal(fields.firmwareMode.value, 'PtpL3Only');
assert.equal(fields.ptpUdpIpv4.value, '192.0.2.20');
assert.equal(fields.ptpUdpIpv6.value, 'fe80::20');
assert.equal(fields.ptpUdpDomain.value, '17');
assert.equal(fields.ptpUdpWithdrawal.value, '25000');
profileContext.loadProfile(null);
assert.equal(fields.ptpUdpIpv4.value, '');
assert.equal(fields.ptpUdpIpv6.value, '');
assert.equal(fields.ptpUdpDomain.value, '0');
assert.equal(fields.ptpUdpWithdrawal.value, '5000');
assert.match(script, /ptp_udp_ipv4:data\.ptpUdpIpv4/);
assert.match(script, /ptp_udp_ipv6:data\.ptpUdpIpv6/);
assert.match(script, /ptp_udp_domain:data\.ptpUdpDomain/);
assert.match(script, /ptp_udp_withdrawal:data\.ptpUdpWithdrawal/);

console.log('PASS: generated web controls, profile loading and live status: 3 groups');
