# F-024 — Doctor Daily Appointment Schedule Deviations

1. Cancelled appointments are excluded from the daily schedule.
2. The response uses a numeric appointmentId, while the handover example specifies a string.
3. The slotStart value uses the format "YYYY-MM-DD HH:MM", while the handover example uses a "T" separator.
4. The response includes doctorId in addition to the other appointment fields.

These differences should be reviewed against the expected API response contract.

## F-061 – Administrator Audit Log Filtering

**1. Audit API response format**

- The Software Architecture and Design specification describes the `GET /api/v1/audit` response as a JSON array of audit entries.
- The implementation returns a JSON object containing an `entries` array:
  ```json
  {
    "entries": [
      {
        "userId": 18,
        "username": "dr.rao",
        "role": "Doctor",
        "action": "LOGIN",
        "recordId": "18",
        "timestamp": "2026-10-30T06:27:53.884626Z",
        "sourceIp": "127.0.0.1"
      }
    ]
  }
  ```
- This format is retained for compatibility with the existing API response structure. The `username` field is included to make the user filter results identifiable.

**2. Audit log result limit**

- The audit log query applies `LIMIT 1000` and returns at most the 1000 most recent matching entries.
- Results are ordered by timestamp in descending order, with audit ID used as a secondary ordering field.
- This limit is applied to avoid returning an excessively large number of records from the audit log table.
- Consequently, matching records beyond the latest 1000 results are not included in a single response.