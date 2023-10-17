def fix_malformed_ms_marco(schifo: str):
    d = bytes(schifo, "latin1")
    return str(d, "utf8")


print(fix_malformed_ms_marco(u"good luck finding that cohort of ânaÃ¯veâ participants"))
print(fix_malformed_ms_marco(u"Home My MBTIÂ® Personality Type Â® Basics."))
print(fix_malformed_ms_marco(u"AntonÃ­n DvorÃ¡k (1841â1904) Antonin Dvorak was a son of butcher,"))
print(fix_malformed_ms_marco(u"Wikipedia â one of the worldâs great examples of transparent collective behaviour â states"))
print(fix_malformed_ms_marco(u"1 Most Tiny creatures canât attack"))
print(fix_malformed_ms_marco(u"uk â us â. âº HR, WORKPLACE, MANAGEMENT"))