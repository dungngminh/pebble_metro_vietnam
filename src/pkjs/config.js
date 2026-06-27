/* Clay configuration page (phone-side settings). Vietnamese labels, ASCII. */
module.exports = [
  { type: 'heading', defaultValue: 'Metro VN' },
  { type: 'text', defaultValue: 'Choose which lines to track and how many departures to pin to the timeline.' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Tracked lines' },
      { type: 'toggle', messageKey: 'track_catlinh', label: 'Cat Linh - Ha Dong', defaultValue: true },
      { type: 'toggle', messageKey: 'track_nhon', label: 'Nhon - Cau Giay', defaultValue: false },
      { type: 'toggle', messageKey: 'track_benthanh', label: 'Ben Thanh - Suoi Tien', defaultValue: false }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Timeline' },
      { type: 'slider', messageKey: 'pin_count', label: 'Departures to pin', defaultValue: 4, min: 0, max: 6, step: 1 },
      { type: 'toggle', messageKey: 'reminders', label: 'Remind before arrival', defaultValue: true }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Appearance' },
      { type: 'toggle', messageKey: 'show_splash', label: 'Show intro animation', defaultValue: true }
    ]
  },
  { type: 'submit', defaultValue: 'Save' }
];
