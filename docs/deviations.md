# F-024 - Doctor Daily Appointment Schedule Deviations

1. Cancelled appointments are excluded from the daily schedule.
2. The response uses a numeric appointmentId, while the handover example specifies a string.
3. The slotStart value uses the format "YYYY-MM-DD HH:MM", while the handover example uses a "T" separator.
4. The response includes doctorId in addition to the other appointment fields.

These differences should be reviewed against the expected API response contract.

## F-042 - Payment Receipt Generation

- Payment recording uses the SAD-specified `POST /api/v1/bills/{billId}/payments` route. The previous flat `POST /api/v1/payments` route remains available for compatibility.
- The SAD does not define a receipt-retrieval endpoint. `GET /api/v1/payments/{paymentId}/receipt` is an additional retrieval route that returns print-ready HTML.
- Receipt dates are shown in IST (`DD-MM-YYYY HH:MM`), converted from the stored `paid_at` TIMESTAMPTZ.
- Receipt balance is computed as of that payment, so a reprint always shows the original figure.
