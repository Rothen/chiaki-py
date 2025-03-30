from typing import TypeVar
from chiaki_py import EventSource
from reactivex.subject import Subject

try:
    pass
except Exception as e:
    print(e)

T = TypeVar('T')

subj = Subject[int]()
subj.subscribe(lambda x: print(x), lambda x: print(x), lambda: print('completed'))
s: EventSource = EventSource()
s.subscribe(lambda x: print(x), lambda x: print(type(x)), lambda: print('completed'))