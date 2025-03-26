from typing import TypeVar, Generic
from chiaki_py import EventSource
from reactivex.subject import Subject

T = TypeVar('T')

class ChiakiSubject(Generic[T], Subject[T]):
    def __init__(self):
        super().__init__()
        self.event_source = EventSource()
        self.event_source.set_on_next(self.on_next)
        self.event_source.set_on_error(self.on_error)
        self.event_source.set_on_completed(self.on_completed)

sub = ChiakiSubject[int]()
sub.subscribe()
sub.on_error