//
// Calcola i valori minimo, medio e massimo
// di concentrazione del particolato
// su tutta la serie storica
// con campionamento 1h
// -------------------------------------------
// Nota: risultati visibili tramite "Raw Data"
// (Non genera grafico su Data Explorer)
//
// Query config
date_start   = "2021-12-30T16:00:00.0Z"
date_stop    = "2022-01-14T23:00:00.0Z"

// Concentraz. PM2,5 per massa
field_name  = "mass_of_pm2"

// Concentraz. PM10 per massa
// field_name  = "mass_of_pm10"


min = from(bucket: "test")
  |> range(start: time(v: date_start), stop: time(v: date_stop))
  |> filter(fn: (r) => r["_measurement"] == "airq_monitor_tambone")
  |> filter(fn: (r) => r["_field"] ==  field_name)
  |> aggregateWindow(every: 1h, fn: last, createEmpty: false)
  |> min()
  |> yield(name: "minimo")

med = from(bucket: "test")
  |> range(start: time(v: date_start), stop: time(v: date_stop))
  |> filter(fn: (r) => r["_measurement"] == "airq_monitor_tambone")
  |> filter(fn: (r) => r["_field"] ==  field_name)
  |> aggregateWindow(every: 1h, fn: last, createEmpty: false)
  |> mean()
  |> yield(name: "medio")

max = from(bucket: "test")
  |> range(start: time(v: date_start), stop: time(v: date_stop))
  |> filter(fn: (r) => r["_measurement"] == "airq_monitor_tambone")
  |> filter(fn: (r) => r["_field"] ==  field_name)
  |> aggregateWindow(every: 1h, fn: last, createEmpty: false)
  |> max()
  |> yield(name: "massimo")
