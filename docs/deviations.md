# F-024 — Doctor Daily Appointment Schedule Deviations

1. Cancelled appointments are excluded from the daily schedule.
2. The response uses a numeric appointmentId, while the handover example specifies a string.
3. The slotStart value uses the format "YYYY-MM-DD HH:MM", while the handover example uses a "T" separator.
4. The response includes doctorId in addition to the other appointment fields.

These differences should be reviewed against the expected API response contract.
