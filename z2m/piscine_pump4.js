const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const m = require('zigbee-herdsman-converters/lib/modernExtend');
const Zcl = require('zigbee-herdsman').Zcl;
const e = exposes.presets;
const ea = exposes.access;

const MANUFACTURER_CODE = 0x1234;

// Custom cluster 0xFC00 — must match specs/02-zigbee-model.md
const piscinePumpCluster = {
    ID: 0xfc00,
    manufacturerCode: MANUFACTURER_CODE,
    attributes: {
        direction: {ID: 0x0000, type: Zcl.DataType.ENUM8, write: true},
        doseDurationS: {ID: 0x0001, type: Zcl.DataType.UINT16},
        doseRemainingS: {ID: 0x0002, type: Zcl.DataType.UINT16, report: true},
        fillTime1lMinS: {ID: 0x0003, type: Zcl.DataType.UINT16, write: true},
        fillTime1lMaxS: {ID: 0x0004, type: Zcl.DataType.UINT16, write: true},
    },
    commands: {
        startDose: {
            ID: 0x00,
            parameters: [
                {name: 'duration_s', type: Zcl.DataType.UINT16},
                {name: 'level', type: Zcl.DataType.UINT8},
                {name: 'direction', type: Zcl.DataType.UINT8},
            ],
        },
        stopDose: {ID: 0x01, parameters: []},
    },
    commandsResponse: {},
};

const fzPiscinePump = {
    cluster: 'piscinePump',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const ep = `pump${msg.endpoint.ID}`;
        const payload = {};
        if (msg.data.direction !== undefined) {
            payload[`direction_${ep}`] = msg.data.direction === 0 ? 'forward' : 'reverse';
        }
        if (msg.data.doseDurationS !== undefined) {
            payload[`dose_duration_${ep}`] = msg.data.doseDurationS;
        }
        if (msg.data.doseRemainingS !== undefined) {
            payload[`dose_remaining_${ep}`] = msg.data.doseRemainingS;
        }
        if (msg.data.fillTime1lMinS !== undefined) {
            payload[`fill_time_1l_min_${ep}`] = msg.data.fillTime1lMinS;
        }
        if (msg.data.fillTime1lMaxS !== undefined) {
            payload[`fill_time_1l_max_${ep}`] = msg.data.fillTime1lMaxS;
        }
        return payload;
    },
};

// Speed is exposed as 0-100 %; on the wire it is the ZCL level 0-254
const pctToLevel = (pct) => Math.round(Math.max(0, Math.min(100, Number(pct))) * 254 / 100);
const levelToPct = (level) => Math.round(Number(level) * 100 / 254);

// CurrentLevel reports -> speed_<pumpN> (in %)
const fzSpeed = {
    cluster: 'genLevelCtrl',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.data.currentLevel !== undefined) {
            return {[`speed_pump${msg.endpoint.ID}`]: levelToPct(msg.data.currentLevel)};
        }
    },
};

const tzSpeed = {
    key: ['speed'],
    convertSet: async (entity, key, value, meta) => {
        const level = pctToLevel(value);
        await entity.command('genLevelCtrl', 'moveToLevel', {level, transtime: 0});
        return {state: {[`speed_${meta.endpoint_name}`]: levelToPct(level)}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('genLevelCtrl', ['currentLevel']);
    },
};

const tzDirection = {
    key: ['direction'],
    convertSet: async (entity, key, value, meta) => {
        const direction = value === 'reverse' ? 1 : 0;
        await entity.write('piscinePump', {direction}, {manufacturerCode: MANUFACTURER_CODE});
        return {state: {[`direction_${meta.endpoint_name}`]: value}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('piscinePump', ['direction'], {manufacturerCode: MANUFACTURER_CODE});
    },
};

// dose_duration is converter-side state only; sent with startDose
const tzDoseDuration = {
    key: ['dose_duration'],
    convertSet: async (entity, key, value, meta) => {
        const duration = Math.max(1, Math.min(3600, Number(value)));
        return {state: {[`dose_duration_${meta.endpoint_name}`]: duration}};
    },
};

const tzDose = {
    key: ['dose'],
    convertSet: async (entity, key, value, meta) => {
        if (value !== 'START') return;
        const ep = meta.endpoint_name;
        const durationState = Number(meta.state[`dose_duration_${ep}`]);
        const duration_s = Number.isFinite(durationState) && durationState >= 1 ? durationState : 60;
        // Fall back to full speed only when speed was never set; an explicit 0 % is honoured (= stop)
        const pctState = Number(meta.state[`speed_${ep}`]);
        const level = Number.isFinite(pctState) ? pctToLevel(pctState) : 254;
        const dirState = meta.state[`direction_${ep}`];
        const direction = dirState === 'reverse' ? 1 : 0;
        await entity.command('piscinePump', 'startDose', {duration_s, level, direction}, {
            manufacturerCode: MANUFACTURER_CODE,
        });
        return {state: {[`state_${ep}`]: 'ON'}};
    },
};

const tzFillTime1lMin = {
    key: ['fill_time_1l_min'],
    convertSet: async (entity, key, value, meta) => {
        const val = Math.max(0, Math.min(65535, Number(value)));
        await entity.write('piscinePump', {fillTime1lMinS: val}, {manufacturerCode: MANUFACTURER_CODE});
        return {state: {[`fill_time_1l_min_${meta.endpoint_name}`]: val}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('piscinePump', ['fillTime1lMinS'], {manufacturerCode: MANUFACTURER_CODE});
    },
};

const tzFillTime1lMax = {
    key: ['fill_time_1l_max'],
    convertSet: async (entity, key, value, meta) => {
        const val = Math.max(0, Math.min(65535, Number(value)));
        await entity.write('piscinePump', {fillTime1lMaxS: val}, {manufacturerCode: MANUFACTURER_CODE});
        return {state: {[`fill_time_1l_max_${meta.endpoint_name}`]: val}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('piscinePump', ['fillTime1lMaxS'], {manufacturerCode: MANUFACTURER_CODE});
    },
};

const pumpExposes = (ep) => [
    e.switch().withEndpoint(ep),
    exposes.numeric('speed', ea.ALL).withEndpoint(ep).withValueMin(0).withValueMax(100).withUnit('%')
        .withDescription('Pump speed, 0% = stop; 1-100% spans the motor usable range'),
    exposes.enum('direction', ea.ALL, ['forward', 'reverse']).withEndpoint(ep)
        .withDescription('Rotation direction, applied on next start'),
    exposes.numeric('dose_duration', ea.STATE_SET).withEndpoint(ep).withValueMin(1).withValueMax(3600)
        .withUnit('s').withDescription('Duration used by the next dose'),
    exposes.numeric('dose_remaining', ea.STATE).withEndpoint(ep).withUnit('s')
        .withDescription('Seconds left in the running dose (0 = idle)'),
    exposes.numeric('fill_time_1l_min', ea.ALL).withEndpoint(ep).withValueMin(0).withValueMax(65535)
        .withUnit('s').withDescription('Calibration: seconds to pump 1 L at minimum running speed (1%)'),
    exposes.numeric('fill_time_1l_max', ea.ALL).withEndpoint(ep).withValueMin(0).withValueMax(65535)
        .withUnit('s').withDescription('Calibration: seconds to pump 1 L at maximum speed (duty 254)'),
    exposes.enum('dose', ea.SET, ['START']).withEndpoint(ep)
        .withDescription('Start a dose with the configured duration, speed and direction'),
];

module.exports = {
    zigbeeModel: ['PISCINE-PUMP4'],
    model: 'PISCINE-PUMP4',
    vendor: 'DIY',
    description: '4-channel peristaltic dosing pump controller',
    extend: [m.deviceAddCustomCluster('piscinePump', piscinePumpCluster)],
    meta: {multiEndpoint: true},
    endpoint: (device) => ({pump1: 1, pump2: 2, pump3: 3, pump4: 4}),
    fromZigbee: [fz.on_off, fzSpeed, fzPiscinePump],
    toZigbee: [tz.on_off, tzSpeed, tzDirection, tzDoseDuration, tzDose, tzFillTime1lMin, tzFillTime1lMax],
    exposes: [
        ...pumpExposes('pump1'),
        ...pumpExposes('pump2'),
        ...pumpExposes('pump3'),
        ...pumpExposes('pump4'),
    ],
    configure: async (device, coordinatorEndpoint, logger) => {
        for (const ep of [1, 2, 3, 4]) {
            const endpoint = device.getEndpoint(ep);
            if (!endpoint) continue;
            await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff', 'piscinePump']);
            await reporting.onOff(endpoint);
            await endpoint.configureReporting('piscinePump', [{
                attribute: 'doseRemainingS',
                minimumReportInterval: 1,
                maximumReportInterval: 60,
                reportableChange: 1,
            }], {manufacturerCode: MANUFACTURER_CODE});
        }
    },
};
